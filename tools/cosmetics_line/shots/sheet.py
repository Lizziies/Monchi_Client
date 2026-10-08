import sys, glob, os
from PIL import Image, ImageDraw

src, out = sys.argv[1], sys.argv[2]
specs = sys.argv[3:]
cols = ["back", "back45", "side", "front", "m_idle", "m_sprint", "m_jump"]
tiles = []
for s in specs:
    row = []
    for c in cols:
        p = os.path.join(src, f"{s}_{c}.png")
        im = Image.open(p).convert("RGB")
        w, h = im.size
        im = im.crop((int(w * 0.12), int(h * 0.06), int(w * 0.88), int(h * 0.86))).resize((300, 316))
        ImageDraw.Draw(im).text((6, 4), f"{s} {c}", fill=(255, 255, 255))
        row.append(im)
    tiles.append(row)
W = 300 * len(cols); H = 316 * len(tiles)
sheet = Image.new("RGB", (W, H))
for y, row in enumerate(tiles):
    for x, im in enumerate(row):
        sheet.paste(im, (x * 300, y * 316))
sheet.save(out)
