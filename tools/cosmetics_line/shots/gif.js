const { chromium } = require('playwright'); const fs = require('fs'), path = require('path');
// env: PAGE (preview html), MODE (idle, walk, sprint, jump), YAW (degrees, 180 is the back)
const three = fs.readFileSync(path.join(__dirname, 'node_modules/three/build/three.min.js'), 'utf8');
const ids = process.argv.slice(2);
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await b.newPage({ viewport: { width: 360, height: 400 } });
  await p.route('**/three.min.js', r => r.fulfill({ body: three, contentType: 'text/javascript' }));
  await p.goto('file://' + path.resolve(process.env.PAGE || 'new.html'));
  await p.addStyleTag({ content: '#stage{position:fixed!important;inset:0!important;z-index:999} #motions{display:none!important} .hint,small{display:none!important}' });
  await p.waitForFunction(() => typeof itemById !== 'undefined' && Object.keys(itemById).length > 0);
  fs.mkdirSync('anim', { recursive: true });
  for (const id of ids) {
    await p.evaluate(([id, mode, yaw, pitch, zoom]) => { applyPreset([id]); state.spin = 0; state.motion = mode; state.yaw = yaw; state.pitch = pitch; state.zoom = zoom; }, [id, process.env.MODE || 'idle', +(process.env.YAW || 150), +(process.env.PITCH || 6), +(process.env.ZOOM || 1)]);
    await p.waitForTimeout(800);
    for (let f = 0; f < +(process.env.FRAMES||16); f++) { await p.locator("#view").screenshot({ path: `anim/${id}_${String(f).padStart(2, "0")}.png` }); await p.waitForTimeout(+(process.env.WAIT||110)); }
  }
  await b.close();
})();
