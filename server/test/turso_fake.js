import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { DatabaseSync } from 'node:sqlite';

// A tiny stand-in for Turso's /v2/pipeline endpoint, backed by node:sqlite.
export async function fakeTurso() {
  const db = new DatabaseSync(':memory:');
  db.exec(readFileSync(new URL('../schema.sql', import.meta.url), 'utf8'));
  const server = createServer(async (req, res) => {
    const chunks = [];
    for await (const c of req) chunks.push(c);
    const body = JSON.parse(Buffer.concat(chunks).toString());
    const results = [];
    for (const r of body.requests) {
      if (r.type === 'close') {
        results.push({ type: 'ok', response: { type: 'close' } });
        continue;
      }
      try {
        const args = r.stmt.args.map((a) => (a.type === 'null' ? null : a.type === 'integer' ? Number(a.value) : a.value));
        const stmt = db.prepare(r.stmt.sql);
        const rows = stmt.all(...args);
        const cols = rows.length ? Object.keys(rows[0]).map((name) => ({ name })) : stmt.columns().map((c) => ({ name: c.name }));
        const out = rows.map((row) =>
          Object.values(row).map((v) => (v === null ? { type: 'null' } : typeof v === 'number' ? { type: 'integer', value: String(v) } : { type: 'text', value: String(v) })),
        );
        results.push({ type: 'ok', response: { type: 'execute', result: { cols, rows: out } } });
      } catch (e) {
        results.push({ type: 'error', error: { message: e.message } });
      }
    }
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ results }));
  });
  await new Promise((ok) => server.listen(0, '127.0.0.1', ok));
  return { url: `http://127.0.0.1:${server.address().port}`, close: () => server.close() };
}
