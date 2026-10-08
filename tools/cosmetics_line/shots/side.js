// usage: node side.js <preview.html> <outdir> <id>...   side view in idle, walk, sprint and jump
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const [, , html, out, ...ids] = process.argv;
const three = fs.readFileSync(path.join(__dirname, 'node_modules/three/build/three.min.js'), 'utf8');

(async () => {
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
  const page = await browser.newPage({ viewport: { width: 420, height: 520 } });
  await page.route('**/three.min.js', r => r.fulfill({ body: three, contentType: 'text/javascript' }));
  await page.goto('file://' + path.resolve(html));
  await page.addStyleTag({ content: '#stage{position:fixed!important;inset:0!important;z-index:999} #motions{display:none!important}' });
  await page.waitForFunction(() => typeof itemById !== 'undefined' && Object.keys(itemById).length > 0);
  for (const id of ids) {
    await page.evaluate(id => { applyPreset([id]); state.spin = 0; state.yaw = 90; state.pitch = 4; }, id);
    for (const mode of ['idle', 'walk', 'sprint', 'jump']) {
      await page.evaluate(m => { state.motion = m; }, mode);
      await page.waitForTimeout(mode === 'jump' ? 600 : 1500);
      await page.locator('#view').screenshot({ path: path.join(out, `${id}_s_${mode}.png`) });
    }
  }
  await browser.close();
})();
