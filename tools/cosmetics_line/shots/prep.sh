#!/bin/bash
# rebuilds the preview pages: new.html (whole figure), head.html and tail.html (camera on the head / the tail root)
R=/home/user/Mochi-3-Client
cd $R && python3 tools/cosmetics_line/build.py >/dev/null || exit 1
python3 tools/cosmetics/preview.py cosmetics/line tools/cosmetics_line/shots/new.html >/dev/null
cd $R/tools/cosmetics_line/shots
mk(){ sed "s/camera.position.set(0, 22,/camera.position.set(0, $1+1,/; s/camera.lookAt(0, 21, 0)/camera.lookAt(0, $1, 0)/" new.html > $2; }
mk 32 head.html; mk 14 tail.html
