import { readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { handle, sweep } from './src/api.js';
import { tursoStore } from './src/store_turso.js';

const { TURSO_URL, TURSO_TOKEN, ADMIN_KEY = '', PORT = '8787' } = process.env;
if (!TURSO_URL || !TURSO_TOKEN) {
  console.error('set TURSO_URL and TURSO_TOKEN');
  process.exit(1);
}

const store = tursoStore({ url: TURSO_URL, token: TURSO_TOKEN });
await store.setup(readFileSync(new URL('./schema.sql', import.meta.url), 'utf8'));
setInterval(() => sweep(store).catch((e) => console.error('sweep', e.message)), 6 * 3600 * 1000);

const env = { ADMIN_KEY };

createServer(async (req, res) => {
  try {
    const chunks = [];
    let size = 0;
    for await (const c of req) {
      size += c.length;
      if (size > 16 * 1024) {
        res.writeHead(413, { 'content-type': 'application/json' });
        res.end('{"error":"body"}');
        return;
      }
      chunks.push(c);
    }
    const request = new Request(`http://${req.headers.host ?? 'localhost'}${req.url}`, {
      method: req.method,
      headers: { ...req.headers, 'cf-connecting-ip': String((process.env.TRUST_PROXY === '1' ? req.headers['x-forwarded-for'] : '') || req.socket.remoteAddress || '').split(',')[0].trim() },
      body: req.method === 'GET' || req.method === 'HEAD' ? undefined : Buffer.concat(chunks),
    });
    const response = await handle(request, env, store);
    res.writeHead(response.status, Object.fromEntries(response.headers));
    res.end(Buffer.from(await response.arrayBuffer()));
  } catch (e) {
    console.error(e.message);
    res.writeHead(500, { 'content-type': 'application/json' });
    res.end('{"error":"server"}');
  }
}).listen(Number(PORT), () => console.log(`mochi online on :${PORT}`));
