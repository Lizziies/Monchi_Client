const row = (r) => (r ? { ...r, style: JSON.parse(r.style), worn: JSON.parse(r.worn), visible: !!r.visible, role: r.role ?? '' } : null);

// db: { one(sql, args), all(sql, args), run(sql, args), batch([[sql, args], ...]) }
export function sqlStore(db, counts = new Map()) {
  // databases created before roles existed get the column the first time they are used
  let migrated = false;
  async function migrate() {
    if (migrated) return;
    migrated = true;
    try {
      await db.run("ALTER TABLE players ADD COLUMN role TEXT NOT NULL DEFAULT ''", []);
    } catch {
      // the column is already there
    }
  }

  return {
    async player(key) {
      await migrate();
      return row(await db.one('SELECT * FROM players WHERE key = ?', [key]));
    },
    async savePlayer(p) {
      await migrate();
      await db.run(
        `INSERT INTO players (key, name, secret, style, worn, visible, server, client, seen, created, role)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
         ON CONFLICT(key) DO UPDATE SET name = excluded.name, secret = excluded.secret, style = excluded.style, worn = excluded.worn,
           visible = excluded.visible, server = excluded.server, client = excluded.client, seen = excluded.seen, role = excluded.role`,
        [p.key, p.name, p.secret, JSON.stringify(p.style), JSON.stringify(p.worn), p.visible ? 1 : 0, p.server, p.client, p.seen, p.created, p.role ?? ''],
      );
    },
    async removePlayer(key) {
      await db.batch([
        ['DELETE FROM sessions WHERE key = ?', [key]],
        ['DELETE FROM players WHERE key = ?', [key]],
      ]);
    },
    async session(token) {
      return db.one('SELECT key, expires FROM sessions WHERE token = ?', [token]);
    },
    async saveSession(token, key, expires) {
      await db.batch([
        ['INSERT OR REPLACE INTO sessions (token, key, expires) VALUES (?, ?, ?)', [token, key, expires]],
        ['DELETE FROM sessions WHERE key = ? AND token NOT IN (SELECT token FROM sessions WHERE key = ? ORDER BY expires DESC LIMIT 3)', [key, key]],
      ]);
    },
    async removeSession(token) {
      await db.run('DELETE FROM sessions WHERE token = ?', [token]);
    },
    async lookup(keys, since) {
      if (!keys.length) return [];
      const marks = keys.map(() => '?').join(',');
      const rows = await db.all(`SELECT * FROM players WHERE key IN (${marks}) AND visible = 1 AND seen >= ?`, [...keys, since]);
      return rows.map(row);
    },
    async count(since) {
      const r = await db.one('SELECT COUNT(*) AS n FROM players WHERE visible = 1 AND seen >= ?', [since]);
      return r ? Number(r.n) : 0;
    },
    async isBlocked(key) {
      return !!(await db.one('SELECT 1 AS x FROM blocked WHERE key = ?', [key]));
    },
    async block(key, reason) {
      await db.batch([
        ['INSERT OR REPLACE INTO blocked (key, reason) VALUES (?, ?)', [key, reason]],
        ['DELETE FROM sessions WHERE key = ?', [key]],
        ['DELETE FROM players WHERE key = ?', [key]],
      ]);
    },
    async hit(bucket, limit, windowSec, now) {
      const slot = `${bucket}:${Math.floor(now / windowSec)}`;
      const n = (counts.get(slot) ?? 0) + 1;
      counts.set(slot, n);
      if (counts.size > 5000) counts.clear();
      return n <= limit;
    },
    async sweep(before) {
      await db.batch([
        ["DELETE FROM players WHERE seen < ? AND role = ''", [before]],
        ['DELETE FROM sessions WHERE expires < ?', [before]],
      ]);
    },
  };
}
