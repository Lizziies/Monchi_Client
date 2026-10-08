// usage: node shot.js <preview.html> <outdir> <id> [<id>...]   (ids joined by + are worn together)
// env: ZOOM (0.6 to 2.2), YAWS (comma list of yaw angles instead of back, back-45, side, front)
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const [, , html, out, ...ids] = process.argv;
const three = fs.readFileSync(path.join(__dirname, 'node_modules/three/build/three.min.js'), 'utf8');
const views = process.env.YAWS ? process.env.YAWS.split(',').map(v => [`y${v}`, +v]) : [['back', 180], ['back45', 140], ['side', 90], ['front', 0]];

(async () => {
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
  const page = await browser.newPage({ viewport: { width: 520, height: 560 } });
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  await page.route('**/three.min.js', r => r.fulfill({ body: three, contentType: 'text/javascript' }));
  await page.goto('file://' + path.resolve(html));
  await page.addStyleTag({ content: '#stage{position:fixed!important;inset:0!important;z-index:999;border-radius:0!important} #motions{display:none!important}' });
  await page.waitForFunction(() => typeof itemById !== 'undefined' && Object.keys(itemById).length > 0);
  for (const spec of ids) {
    const list = spec.split('+');
    const zoom = +(process.env.ZOOM || 1);
    await page.evaluate(([l, z]) => { applyPreset(l); state.spin = 0; state.motion = 'idle'; state.pitch = 8; state.zoom = z; window.dispatchEvent(new Event('resize')); }, [list, zoom]);
    await page.setViewportSize({ width: 521, height: 560 }); await page.waitForTimeout(100); await page.setViewportSize({ width: 520, height: 560 });
    await page.waitForTimeout(900);
    for (const [name, yaw] of views) {
      await page.evaluate(y => { state.yaw = y; }, yaw);
      await page.waitForTimeout(250);
      await page.locator('#view').screenshot({ path: path.join(out, `${spec}_${name}.png`) });
    }
    await page.evaluate(() => { state.yaw = 180; });
    for (const [mode, t] of [['idle', 700], ['sprint', 1400], ['jump', 500]]) {
      await page.evaluate(m => { state.motion = m; }, mode);
      await page.waitForTimeout(t);
      await page.locator('#view').screenshot({ path: path.join(out, `${spec}_m_${mode}.png`) });
    }
  }
  if (errors.length) console.log('ERRORS:\n' + errors.join('\n'));
  await browser.close();
})();
