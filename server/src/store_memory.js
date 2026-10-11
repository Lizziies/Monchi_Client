export function memoryStore() {
  const players = new Map();
  const sessions = new Map();
  const blocked = new Map();
  const hits = new Map();

  return {
    async player(key) {
      return players.get(key) ?? null;
    },
    async savePlayer(p) {
      players.set(p.key, { ...p });
    },
    async removePlayer(key) {
      players.delete(key);
      for (const [token, s] of sessions) if (s.key === key) sessions.delete(token);
    },
    async session(token) {
      return sessions.get(token) ?? null;
    },
    async saveSession(token, key, expires) {
      sessions.set(token, { key, expires });
      const mine = [...sessions].filter(([, s]) => s.key === key).sort((a, b) => a[1].expires - b[1].expires);
      for (const [old] of mine.slice(0, Math.max(0, mine.length - 3))) sessions.delete(old);
    },
    async removeSession(token) {
      sessions.delete(token);
    },
    async lookup(keys, since) {
      return keys.map((k) => players.get(k)).filter((p) => p && p.visible && p.seen >= since);
    },
    async count(since) {
      return [...players.values()].filter((p) => p.visible && p.seen >= since).length;
    },
    async isBlocked(key) {
      return blocked.has(key);
    },
    async block(key, reason) {
      blocked.set(key, reason);
      players.delete(key);
    },
    async hit(bucket, limit, windowSec, now) {
      const slot = `${bucket}:${Math.floor(now / windowSec)}`;
      const n = (hits.get(slot) ?? 0) + 1;
      hits.set(slot, n);
      if (hits.size > 5000) hits.clear();
      return n <= limit;
    },
    async sweep(before) {
      for (const [k, p] of players) if (p.seen < before && !p.role) players.delete(k);
      for (const [t, s] of sessions) if (s.expires < before) sessions.delete(t);
    },
  };
}
