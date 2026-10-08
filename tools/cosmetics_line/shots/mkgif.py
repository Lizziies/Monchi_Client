import sys, glob
from PIL import Image
out, prefix = sys.argv[1], sys.argv[2]
frames = [Image.open(f).convert("P", palette=Image.ADAPTIVE) for f in sorted(glob.glob(f"anim/{prefix}_*.png"))]
frames[0].save(out, save_all=True, append_images=frames[1:], duration=110, loop=0, optimize=True)
