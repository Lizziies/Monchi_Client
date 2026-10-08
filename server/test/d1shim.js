import { readFileSync } from 'node:fs';
import { DatabaseSync } from 'node:sqlite';

export function fakeEnv() {
  const db = new DatabaseSync(':memory:');
  db.exec(readFileSync(new URL('../schema.sql', import.meta.url), 'utf8'));
  const kv = new Map();

  const statement = (sql, args = []) => ({
    bind: (...next) => statement(sql, next),
    first: async () => db.prepare(sql).get(...args) ?? null,
    all: async () => ({ results: db.prepare(sql).all(...args) }),
    run: async () => db.prepare(sql).run(...args),
    exec: () => db.prepare(sql).run(...args),
  });

  return {
    DB: {
      prepare: (sql) => statement(sql),
      batch: async (list) => list.map((s) => s.exec()),
    },
    LIMITS: {
      get: async (k) => kv.get(k) ?? null,
      put: async (k, v) => void kv.set(k, v),
    },
  };
}
