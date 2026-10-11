import { sqlStore } from './store_sql.js';

const arg = (v) => {
  if (v === null || v === undefined) return { type: 'null' };
  if (typeof v === 'number') return Number.isInteger(v) ? { type: 'integer', value: String(v) } : { type: 'float', value: v };
  return { type: 'text', value: String(v) };
};

const cell = (c) => {
  if (!c || c.type === 'null') return null;
  if (c.type === 'integer') return Number(c.value);
  if (c.type === 'float') return Number(c.value);
  return c.value;
};

// Turso over plain HTTP (the /v2/pipeline endpoint), no client library needed.
export function tursoStore({ url, token }) {
  const endpoint = url.replace(/^libsql:/, 'https:').replace(/\/+$/, '') + '/v2/pipeline';

  async function send(list) {
    const requests = list.map(([sql, args = []]) => ({ type: 'execute', stmt: { sql, args: args.map(arg) } }));
    requests.push({ type: 'close' });
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { authorization: `Bearer ${token}`, 'content-type': 'application/json' },
      body: JSON.stringify({ requests }),
    });
    if (!res.ok) throw new Error(`turso ${res.status}: ${(await res.text()).slice(0, 200)}`);
    const data = await res.json();
    return data.results.slice(0, list.length).map((r) => {
      if (r.type === 'error') throw new Error(`turso: ${r.error?.message ?? 'error'}`);
      const result = r.response.result;
      const names = result.cols.map((c) => c.name);
      return result.rows.map((row) => Object.fromEntries(row.map((c, i) => [names[i], cell(c)])));
    });
  }

  const store = sqlStore({
    one: async (sql, args) => (await send([[sql, args]]))[0][0] ?? null,
    all: async (sql, args) => (await send([[sql, args]]))[0],
    run: async (sql, args) => void (await send([[sql, args]])),
    batch: async (list) => void (await send(list)),
  });
  store.setup = async (schema) => {
    const parts = schema.split(';').map((part) => part.trim()).filter(Boolean);
    await send(parts.map((sql) => [sql, []]));
  };
  return store;
}
