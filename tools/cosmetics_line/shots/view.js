// usage: PAGE=x.html YAWS=30,150 PITCH=18 ZOOM=3 MODE=idle node view.js <outprefix> <id>[+<id>...]
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const three = fs.readFileSync(path.join(__dirname, 'node_modules/three/build/three.min.js'), 'utf8');
const [, , out, spec] = process.argv;
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
  const p = await b.newPage({ viewport: { width: 420, height: 420 } });
  await p.route('**/three.min.js', r => r.fulfill({ body: three, contentType: 'text/javascript' }));
  await p.goto('file://' + path.resolve(process.env.PAGE || 'new.html'));
  await p.addStyleTag({ content: '#stage{position:fixed!important;inset:0!important;z-index:999;border-radius:0!important} #motions{display:none!important} .hint,small{display:none!important}' });
  await p.waitForFunction(() => typeof itemById !== 'undefined' && Object.keys(itemById).length > 0);
  await p.evaluate(([l, z, pitch, mode]) => { applyPreset(l); state.spin = 0; state.motion = mode; state.pitch = pitch; state.zoom = z; window.dispatchEvent(new Event('resize')); }, [spec.split('+'), +(process.env.ZOOM || 1), +(process.env.PITCH || 10), process.env.MODE || 'idle']);
  await p.setViewportSize({ width: 421, height: 420 }); await p.waitForTimeout(100); await p.setViewportSize({ width: 420, height: 420 });
  await p.waitForTimeout(900);
  for (const y of (process.env.YAWS || '30,330,90,150').split(',')) {
    await p.evaluate(v => { state.yaw = v; }, +y);
    await p.waitForTimeout(300);
    await p.locator('#view').screenshot({ path: `${out}_${y}.png` });
  }
  await b.close();
})();
