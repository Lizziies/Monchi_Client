import sys
from PIL import Image
out, *files = sys.argv[1:]
ims = [Image.open(f) for f in files]
w, h = ims[0].size
sh = Image.new("RGB", (w * len(ims), h))
for i, im in enumerate(ims): sh.paste(im, (i * w, 0))
sh.save(out)
