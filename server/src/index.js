import { handle, sweep } from './api.js';
import { d1Store } from './store_d1.js';

export default {
  fetch(request, env) {
    return handle(request, env, d1Store(env));
  },
  scheduled(event, env, ctx) {
    ctx.waitUntil(sweep(d1Store(env)));
  },
};
