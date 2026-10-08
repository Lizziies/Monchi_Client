import { createServer } from 'node:http';
import { handle } from './src/api.js';
import { memoryStore } from './src/store_memory.js';

const port = Number(process.env.PORT ?? 8787);
const env = { ADMIN_KEY: process.env.ADMIN_KEY ?? '' };
const store = memoryStore();

createServer(async (req, res) => {
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
  const body = Buffer.concat(chunks);
  const request = new Request(`http://${req.headers.host}${req.url}`, {
    method: req.method,
    headers: req.headers,
    body: req.method === 'GET' || req.method === 'HEAD' ? undefined : body,
  });
  const response = await handle(request, env, store);
  console.log(req.method, req.url, response.status);
  res.writeHead(response.status, Object.fromEntries(response.headers));
  res.end(Buffer.from(await response.arrayBuffer()));
}).listen(port, '127.0.0.1', () => console.log(`mochi online dev server on http://127.0.0.1:${port}`));
