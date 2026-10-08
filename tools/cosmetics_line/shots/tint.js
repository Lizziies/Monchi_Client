// usage: node tint.js <preview.html> <outdir> <id>...   back view in four colour sets per item
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const [, , html, out, ...ids] = process.argv;
const three = fs.readFileSync(path.join(__dirname, 'node_modules/three/build/three.min.js'), 'utf8');
const sets = [['#ffffff', '#ffd77a', '#9aa3b5'], ['#222228', '#ff4d6d', '#5d5570'], ['#e23b3b', '#ffe08a', '#3a1410'], ['#3b7bff', '#bff3ff', '#1d1238']];

(async () => {
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
  const page = await browser.newPage({ viewport: { width: 520, height: 560 } });
  await page.route('**/three.min.js', r => r.fulfill({ body: three, contentType: 'text/javascript' }));
  await page.goto('file://' + path.resolve(html));
  await page.addStyleTag({ content: '#stage{position:fixed!important;inset:0!important;z-index:999} #motions{display:none!important}' });
  await page.waitForFunction(() => typeof itemById !== 'undefined' && Object.keys(itemById).length > 0);
  for (const id of ids)
    for (const [k, set] of sets.entries()) {
      await page.evaluate(([id, set]) => {
        applyPreset([id]);
        state.tints[id] = itemById[id].tints.map((_, i) => set[i % set.length]);
        state.spin = 0; state.motion = 'idle'; state.yaw = 180; state.pitch = 8;
      }, [id, set]);
      await page.waitForTimeout(500);
      await page.locator('#view').screenshot({ path: path.join(out, `${id}_tint${k}.png`) });
    }
  await browser.close();
})();
