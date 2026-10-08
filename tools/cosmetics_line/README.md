# cosmetics_line

Generator for the new cosmetic line in `cosmetics/line/`. Python 3 with numpy and Pillow.

```
python3 tools/cosmetics_line/build.py
```

Every texture is painted in code (`paint.py`), `item.py` packs the atlas and writes `item.json`. Pets (`pets.py`) are textured boxes in the Minecraft layout on a bone tree (`parent`): a root at the player's centre trails behind on a spring, legs use `walk`, the shoulder bunny `hop`. Bandanas (`bandanas.py`) put four cards around the head and hang two spring ends from a knot. Capes (`capes.py`) are eight cloth strips per layer; every layer is its own cloth bone with the same strips, physics and idle sway, so pattern and cloth move as one, and the tilt keeps the sway behind the body. Wings go through `wing_pair`, which puts every layer on the same root so nothing can come loose, and searches the highest root that stays clear of the player. `clearance.py` turns each item through idle, both ends of the flap, walk, sprint, sneak and jump and fails the build if a visible texel ends up in the head, torso or arms.

## Looking at the result

`shots/` renders items with the HTML preview in headless Chromium, so you can check them without the game:

```
cd tools/cosmetics_line/shots && npm install
python3 ../../cosmetics/preview.py ../../../cosmetics/line new.html
node shot.js new.html out crystal_wings web_wings     # back, back-45, side, front, idle, sprint, jump
python3 sheet.py out sheet.png crystal_wings web_wings
node side.js new.html out moonlit_cape                 # side view in idle, walk, sprint, jump
PAGE=new.html MODE=walk node anim.js moonlit_cape       # 16 frames into anim/
```

The preview is not the client renderer (`dll/src/cosmetics/Preview.cpp`); check new items in the Mochi menu as well.

`tools/cosmetics/native/` builds the client's own loader and renderer for Linux and renders items to PPM files, so the C++ side can be checked here too:

```
tools/cosmetics/native/build.sh /tmp/render
MOCHI_ROOT=<dir containing cosmetics/> UNIT=8 FOCUS=16 EVERY=5 /tmp/render out/cat 140 walk 160 pet_cat
```
