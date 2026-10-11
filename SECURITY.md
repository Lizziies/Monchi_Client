# Security

Monchi is beta software that loads its own code into Minecraft, so there can still be bugs and crashes.

If you find a security problem, please report it privately through GitHub's vulnerability reporting on this repo. If that isn't switched on, open an issue and ask for a private way to reach me, without describing the problem there. Never post passwords, account names, Windows paths, crash dumps or other players' data in a public issue.

## Things to know

- The launcher runs code on your PC. Only use builds from someone you trust.
- Updates are signed. The launcher checks an ECDSA P-256 signature on the checksum list of each release before it installs anything, and rejects missing, changed or replayed lists. The public key is in the source and the launcher, the private key never is. This works from 0.1.1 on; older launchers update once the old way first. It's separate from Windows code signing, so SmartScreen can still warn about the exe.
- GitHub Actions only build test artifacts, releases aren't made by CI.
- Monchi Online knows your install by a random secret, it can't prove that you own a gamertag. Whoever claims a free name first gets it, so don't treat badges as proof of who someone is.
- The Online rate limits only count per server instance. A public server also needs rate limiting at the edge. The standalone server only trusts forwarded IP headers with `TRUST_PROXY=1`, so only set that behind a proxy that replaces them.
- The local dev server only listens on loopback. Don't run it as a public server.
- Online secrets and session tokens stay on your PC and never belong in the repo. Outside of local testing the client only talks HTTPS and doesn't follow redirects. Admin keys go into the server's secrets.
- Request bodies are capped at 16 KiB before parsing and the client reads responses into fixed-size buffers.
- LeviLauncher is pinned to v0.3.13 and checked against its SHA-256 before it runs. The adapted Flarial code doesn't send telemetry or read a machine ID, and the old Flarial API calls are blocked.
- When a Minecraft update moves things around, modules whose signatures are missing turn off instead of crashing.

`tools/check_publish.py` scans the repo for personal data and secrets before anything gets published, and `.gitleaks.toml` is set up for a Gitleaks scan. It only skips one ImGui line that looks like a key but isn't.
