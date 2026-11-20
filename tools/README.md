# Pakal Scene Tools

## Sprite atlases (`pakal_atlas`)

`pakal_atlas/pakal_atlas.py` packs sprite frames into a texture atlas with a polygon mesh cut to each frame, for `Pakal::SpriteAtlas`
(`pack`, `info`, `preview`; `pakal_atlas_editor.py` is a window to edit the meshes by hand, docs/ATLAS.md; needs Pillow, numpy and scipy; tests: `python -m pytest tools/pakal_atlas`). Format and use: `docs/ATLAS.md`.

## Tiled scene conversion

`pakal_tiled_scene.py` converts the current solid-rectangle Pakal scene format to and from Tiled `.tmj` maps. It is intended for the ContraGame placeholder levels.

Open the generated maps in Tiled:

```bash
tiled examples/ContraGame/Assets/tiled/level1.tmj
tiled examples/ContraGame/Assets/tiled/level2.tmj
```

Generate or refresh Tiled maps from Pakal scene JSON:

```bash
python3 tools/pakal_tiled_scene.py pakal-to-tiled examples/ContraGame/Assets/scenes/level1.json examples/ContraGame/Assets/tiled/level1.tmj
python3 tools/pakal_tiled_scene.py pakal-to-tiled examples/ContraGame/Assets/scenes/level2.json examples/ContraGame/Assets/tiled/level2.tmj
```

Convert an edited Tiled map back to the runtime scene JSON:

```bash
python3 tools/pakal_tiled_scene.py tiled-to-pakal examples/ContraGame/Assets/tiled/level1.tmj examples/ContraGame/Assets/scenes/level1.json
python3 tools/pakal_tiled_scene.py tiled-to-pakal examples/ContraGame/Assets/tiled/level2.tmj examples/ContraGame/Assets/scenes/level2.json
```

Important object names:

- `player`, `goal`, and `camera` are required by `GamePlayState`.
- Enemy names should contain `enemy`, for example `enemy_01`.
- Terrain and platform rectangles can be renamed freely, but keeping `ground_XX` and `platform_XX` makes diffs easier to review.

The converter treats one Tiled grid tile as one Pakal world unit and handles the y-axis flip between Tiled's top-left origin and Pakal's y-up world.
