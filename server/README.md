# Mochi Online service

Small Cloudflare Worker that lets Mochi clients find each other: who runs Mochi on this server, and how their name, tag and cosmetics look. No Minecraft traffic goes through it. The concept is in `docs/ONLINE.md`.

## Run it locally

```
cd server
npm run dev        # http://127.0.0.1:8787, data lives in memory
npm test           # runs the tests against the memory store and against the real SQL (node:sqlite)
```

Point the client at it: Mochi Online, "Service address" = `http://127.0.0.1:8787`.

## Publish it without installing anything

You only need a browser and a free Cloudflare account.

1. Dashboard, Storage & databases, D1, create a database called `mochi-online`. Open its Console tab, paste the content of `schema.sql` and run it.
2. Workers & Pages, Create, start from "Hello World", name it `mochi-online`, deploy, then "Edit code": select everything, paste the content of `dist/worker.js` and deploy again.
3. The worker, Settings, Bindings, add a D1 database: variable name `DB`, database `mochi-online`.
4. Optional: Settings, Variables and secrets, add a secret `ADMIN_KEY` (needed for `/v1/admin/block`). Triggers, Cron, add `17 3 * * *` (the daily cleanup).
5. Open `https://<worker>.<account>.workers.dev/v1/health`. It answers `{"ok":true,"online":0}`.

`dist/worker.js` is generated from `src/` with `node bundle.js`, so after changing the code build it again.

## Publish it on Render with Turso

For people who want a plain Node server. The data lives in Turso (SQLite in the cloud, free plan), so nothing is lost when Render restarts or sleeps.

1. turso.tech: sign up, create a database `mochi-online`. Copy its URL (`libsql://...`) and create a token for it.
2. render.com: New, Web Service, connect the GitHub repo. Root directory `server`, runtime Node, build command empty, start command `node render.js`, instance type Free.
3. Environment variables on Render: `TURSO_URL`, `TURSO_TOKEN`, optional `ADMIN_KEY`. The tables are created on start.
4. A free Render service falls asleep after about 15 minutes without requests. Add a monitor on uptimerobot.com (free): HTTPS, `https://<service>.onrender.com/v1/health`, every 5 minutes.

The Turso adapter talks to Turso's HTTP API with `fetch` and has no dependencies. It is tested against a stand-in built on `node:sqlite`, not against the real Turso yet.

## Publish it with wrangler

Needs Node. Put the database id into `wrangler.toml`, then:

```
cd server
npx wrangler login
npx wrangler d1 create mochi-online
npx wrangler d1 execute mochi-online --remote --file schema.sql
npx wrangler secret put ADMIN_KEY
npx wrangler deploy
```

Enter the printed address in the client as the service address (or make it the default in `dll/src/modules/online/MochiOnline.hpp`).

## Calls

All bodies are JSON, all answers are JSON. Everything except `hello` and `health` needs `Authorization: Bearer <token>` from `hello`.

| Call | What it does |
|---|---|
| `POST /v1/hello` | `{name, secret, client, visible, style, worn}` signs in and returns `{token, ttl, style, worn}`. The `secret` is a random 48 character hex string the client creates once and keeps. The first install that uses a gamertag owns it, others get 403 until it has been silent for 30 days. |
| `POST /v1/profile` | `{visible, style, worn}` saves the look. Anything that is not a known field is dropped, there is no free text. |
| `POST /v1/presence` | `{server}` heartbeat, every 2 minutes, this is also what keeps a user listed. |
| `POST /v1/lookup` | `{names: [...]}` (at most 100) returns `{users: [{name, style, worn}]}` for visible users seen in the last 5 minutes. |
| `POST /v1/bye` | ends the session and hides the user. |
| `POST /v1/forget` | deletes everything stored about the gamertag. |
| `POST /v1/admin/block` | `{name, reason}` with header `X-Admin-Key`, blocks a gamertag. |
| `POST /v1/admin/role` | `{name, role}` with header `X-Admin-Key`, `role` is `owner`, `staff` or empty. The gamertag must have signed in once. Mochi users see `[Owner]` / `[Team]` behind the name in chat and the Tab List. |
| `GET /v1/health` | `{ok, online}` |

`hello` and `lookup` also return `role` (empty for normal users). A role belongs to the install that holds the gamertag: if somebody else reclaims the name after 30 days, the role is gone.

`style` is `{mode: solid|gradient|rainbow|pulse, a, b, speed, heart, heartColor}` with colors as `#rrggbb`. `worn` is a list of `{id, tint: [#rrggbb, ...]}` with at most 8 items.

## What it stores

Gamertag, a hash of the install secret, style, worn cosmetics, server name, client version, last seen. Nothing from chat, no worlds. IP addresses only live in the rate limit counters in memory. Rows nobody has touched for 90 days are deleted by a daily job.

## Limits

Ten `hello` per minute per address, 30 calls per route per minute per gamertag (counted in the worker's memory, best effort), bodies up to 16 KB.

## What someone can and cannot do with it

The service never sees a Microsoft or Xbox sign-in, a password, a token of the game or an address of a player, so nothing in it leads to anybody's Minecraft account. What it holds is public by nature: a gamertag and how its name looks to other Monchi users.

- A gamertag is not proven. Whoever signs in with a name first holds it, so a stranger can take a name that has never used Monchi and dress it up. They get no access to the account, only the look other Monchi users see for that name. One address can take ten new names a day, and a name is given up after 30 silent days.
- A name with a role (`owner`, `staff`) is never handed to another install and is not removed by the cleanup. A free tag that reads like a badge ("Owner", "Monchi Staff", ...) is dropped unless the name has a role.
- The admin calls need `ADMIN_KEY` with at least 24 characters, five tries a minute per address. With a shorter key they are off. The key is a secret of the worker and is in no file of this repository.
- `lookup` tells a signed-in client whether a gamertag it asks for is online with Monchi right now. It never returns the server somebody is on. "Visible" off in the client keeps a name out of it.
- The limits are counted in the worker's memory and start over when Cloudflare restarts it; they slow abuse down, they do not make it impossible.

## Not done yet

- Proof that you own the gamertag (the Xbox sign-in token). Until then the first claim wins, and the 30 day rule is the only way back for someone who lost their secret.
- Reports. There is no free text, so only the admin block exists for now.

## Owner badge

1. Start Minecraft with Mochi Online on once, so the gamertag (for example `<gamertag>`) is signed in and claimed by this PC.
2. Give it the role (the admin key is the `ADMIN_KEY` secret of the worker):

```
curl -X POST https://<worker>/v1/admin/role -H "X-Admin-Key: <ADMIN_KEY>" -H "content-type: application/json" -d "{\"name\":\"<gamertag>\",\"role\":\"owner\"}"
```

Older databases get the `role` column by themselves on the first request after the update.
