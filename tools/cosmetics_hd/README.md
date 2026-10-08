# HD cosmetics (work in progress)

Wings, capes and a few more, drawn with detailed alpha-shaped textures and spring physics. This is a test set and a generator, not the official `cosmetics/` folder (that belongs to the cosmetics chat).

```
python3 tools/cosmetics_hd/tools/build.py          # all items
python3 tools/cosmetics_hd/tools/build.py capes    # one source file
```

`tools/lib.py` has the helpers (canvas, atlas, polygon painting, feather shapes), `src/*.py` has one file per group. The output is `<id>/item.json` and `<id>/tex.png`, plus `index.json`.

The items use format extensions that the loader in `dll/src/cosmetics/` does not read yet: `texel`, `flat`, `mirror`, `tint2`/`mix`, `physics`, `sparkle` and textures up to 256x256. The extensions are described in `docs/HANDOFF.md`; an earlier loader and renderer that implemented all of them is in commit `a3385e5` (`dll/src/modules/cosmetics/`).
