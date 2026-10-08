import { sqlStore } from './store_sql.js';

const limiters = new WeakMap();

export function d1Store(env) {
  const d1 = env.DB;
  if (!limiters.has(d1)) limiters.set(d1, new Map());
  const stmt = (sql, args = []) => d1.prepare(sql).bind(...args);
  return sqlStore(
    {
      one: (sql, args) => stmt(sql, args).first(),
      all: async (sql, args) => (await stmt(sql, args).all()).results,
      run: (sql, args) => stmt(sql, args).run(),
      batch: (list) => d1.batch(list.map(([sql, args]) => stmt(sql, args))),
    },
    limiters.get(d1),
  );
}
