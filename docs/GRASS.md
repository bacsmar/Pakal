# Grass made on the GPU

`GraphicsSystem::create_grass` grows a field of grass blades that the GPU shapes itself. It is meant for wide grassy
ground in any 3D game, in place of alpha-tested grass cards, which are the slowest thing a phone GPU can draw.

Why it is cheap:
- **Solid blades, no alpha test.** A blade is five vertices and three triangles with no texture. Nothing discards, so
  the hidden-surface removal of mobile GPUs (Mali, Adreno, PowerVR) skips the pixels behind them. Measured on a Mali-G77
  in a dense marsh level, masked card clusters cost about 12 ms of a 31 ms frame, while solid low-poly grass cost under 1 ms.
- **A few draws.** A field is cut into square tiles that share one mesh. A frame sorts the visible tiles into four rings
  by distance and draws each ring in one instanced draw: four draws a field.
- **The vertex shader does the work.** It turns each blade's random numbers, its tile's place and the field's maps into
  the blade's height, facing, lean, wind and color. The CPU only lists the tiles in view.

## Using it

```cpp
Pakal::GraphicsSystem::GrassField field;
field.minX = -40; field.maxX = 40; field.minZ = -40; field.maxZ = 40;   // render space: x east, z south, meters
field.groundY = 0.0f;
field.density = coverBytes; field.densityWidth = 256; field.densityHeight = 256;   // 0..255: where it grows
field.heights = groundHeights; field.heightsWidth = 128; field.heightsHeight = 128; // optional rolling ground
field.colorRoot[0] = ...;                                                          // the colors, heights, width...
auto grass = graphics->create_grass(std::move(field));
...
graphics->destroy_grass(grass);   // when the level unloads
```

- **Threading:** any thread may call these; the work is posted to the graphics thread and the data is copied in.
- **Handles:** `0` means no grass (the dummy graphics). A field that cannot be made is logged and draws nothing.
- **Where the reach is measured from:** the focus that `set_scatter_focus` sets, the same as the scattered ground
  cover. For a top-down camera that is what it looks at, not its eye; until a focus is set, the eye.
- **Quality and governor:** `set_grass_scale(reach, density)` scales every field's reach and density, for a quality
  level or the governor. Blades left out by a lower density make the remaining ones wider, so the ground stays covered.
  `set_grass_shadows(false)` drops the blades' read of the sun's shadow (`PAKAL_GRASS_SHADOWS=0` starts with it off).
  On a Mali-G77 the shadow read cost nothing measurable; the cost is the area the blades cover. In a dense marsh
  level a field cost about 3.5 ms at scale 1, and 0.7 ms at `set_grass_scale(0.6, 0.5)`. A start for the quality levels:
  High (0.8, 0.7), Medium (0.6, 0.5), Low (0.45, 0.35).
- **Wind:** `MeshComponent3D_Bgfx::set_wind` drives the grass and the alpha-tested foliage together.

## From a level's grass models

A level built with grass models (clumps and tufts placed by hand or by a level builder) can have them drawn as one field
instead. `TopDown::GrassFromPlants` (game_genres/topdown/view/GrassFromPlants.h) takes a spec as data and works in three steps:
- the models whose name starts with one of `replaces` are not drawn;
- their places give the field its density, counted in cells of `cell` meters and softened over 5 x 5 cells, so a patch
  thins toward its edges;
- the cells with plants give the ground's height.

The rest of the spec is the blades' look: the fields of the table below, by their names in JSON.

```json
{ "replaces": ["Grass_Short", "Grass_Med"], "cell": 2, "full": 0.3,
  "heights": [0.25, 0.6], "width": 0.08, "bladesPerSquareMeter": 60,
  "root": [0.16, 0.27, 0.08], "tip": [0.50, 0.66, 0.24],
  "clumpColors": [ { "color": [0.62, 0.56, 0.30], "share": 0.25 } ], "tuftBlades": 0 }
```

- `full`: the share of the cells around that must hold a plant for the grass to be full there (0.3: about a third).
- **With `LevelRuntime::place_level`:** set `LevelScenery::grass` to the spec. The runtime collects the places, makes
  the field, and destroys it at `end()`. A keyed object (one the game may move, hide or swap) is always kept as a model.
- **A game's own loader:** call `replaces(name)` for each object, `add(x, y, z)` with its render-space place instead of
  making the model, then `create_grass(grass.field())`. `GrassFromPlants::field_from(spec, places)` does the same from
  a list of places.
- The grass models still count for what they do in the game (hiding the player, say): only their drawing is replaced.
- **Quality:** `set_grass_scale` applies to these fields as to any other.

## The field

| Field | Meaning |
|---|---|
| `minX`, `minZ`, `maxX`, `maxZ` | The rectangle on the ground, in render space (meters). |
| `groundY` | The ground's height where `heights` gives none. |
| `density` (`densityWidth` x `densityHeight`) | Bytes over the rectangle, row 0 at `minZ`: the share of the blades kept in each cell. Empty means it grows everywhere. Paint 0 under roads, buildings and water. |
| `heights` (`heightsWidth` x `heightsHeight`) | The ground's height over the rectangle (meters). Kept in 16 bits over its range, blended between texels in the shader. |
| `bladesPerSquareMeter` | Blades where the density is full, near the focus (default 60, with `width` 0.08: on a Mali-G77 fewer, wider blades cost 1.6 ms less than 90 at 0.06 and cover the same from a top-down camera). |
| `heightMin`, `heightMax`, `width` | A blade's size (m). Clumps of noise mix taller and shorter blades. |
| `colorRoot`, `colorTip` | Dark at the root, the tip's color at the top in most clumps. |
| `clumpColors[3]` (`color`, `share`) | Clumps of other colors (dry ochre, dark olive, dying brown): each color has a noise of its own, and about its share of the field takes it, in patches. |
| `deadShare`, `colorDead` | Dead blades scattered among the living ones. |
| `clumpSize`, `clumpStrength` | The clumps' size (m), and how far a clump's height and color depart from the field's: 0 is an even lawn, 1 tall next to short. |
| `tuftBlades`, `tuftSpread` | Blades that fan out from a shared root (0: blades stand alone), and how far they lean out. A field of tufts can stand in for clump models. |
| `reach` | Meters from the focus where the last blades end (default 28). Beyond it, the ground's own texture carries the grass. |
| `thinFrom`, `farDensity` | All blades out to `thinFrom` of the reach (default 0.15), then fewer, down to `farDensity` of them at the reach (default 0.12). The far rings draw each blade as one triangle. |

## How it works

- **The tile mesh** (`BgfxGrass::add`) is a grid of 32 x 32 blades, or 16 x 16 for a sparse field. Its side is set by
  the density, which makes 4.1 m tiles at the default 60 blades per square meter.
- **Blade order:** the blades are stored in rank order. Rank r takes the grid cell whose index is r's bits reversed,
  so any prefix of the list is an even sample of the tile. A ring draws only the prefix its density needs.
- **Fading in and out:** the shader keeps a blade when the field's density there, thinned by distance, is above the
  blade's rank. The same test fades blades in and out smoothly instead of popping. A blade left out gets no height,
  and its triangles fold into a line.
- **Facing:** each blade's facing follows its clump. A blade seen edge-on is turned toward the camera, and its face
  is always the side the camera sees.
- **Lean and wind:** both grow toward the tip, and the blade keeps its length as it bends.
- **Edges:** where the density map thins, the blades are shorter too, so a patch tapers into bare ground instead of
  ending in a wall.
- **Lighting** (`fs_grass`): the sky and ground ambient, a wrapped sun, and darker at the root. The vertex shader
  reads the sun's shadow map once at each blade's root, so a blade in a tree's shade is shaded all along. The grass
  casts no shadow.
- **The instance buffer:** a frame's tiles go in the instance buffer that every instanced draw shares. bgfx grows it
  by chunks, and each chunk takes a vertex buffer handle (4096 in all). In a level that holds nearly all of them, only
  the last chunk's room is left: the tiles that fit are drawn, nearest first, and the rest are counted in
  `gpu.grass_dropped` (logged once a run). The grass is drawn first of the instanced draws, so it takes its few bytes a
  tile before the copies and the cover do. `gpu.vertex_buffers` says how near the limit a level is.

## Not yet

- **The lightmap:** grass on a level with baked light does not read it; the sun's shadow map is all it gets.
- **Interaction:** a pushed-aside trail behind the player, and a wind noise that game code can also sample.
- **Compute:** a compute-shader path on GLES 3.1 and D3D11, culling blade by blade.
- **Levels:** a level's grass models can become a field ("From a level's grass models"), but the level file has no grass
  entry of its own yet. Such an entry would give a rectangle and a density map the level builder paints: zero under
  roads, buildings and water, and the cover rules elsewhere. The editor does not show grass yet either.

The demo: `PakalMeshViewer <model> 10 1 0` with `MESHVIEWER_GRASS=1` (`hills` for rolling ground, `marsh` for patchy
marsh grass in tufts), `MESHVIEWER_TOPDOWN=1` and `MESHVIEWER_GROUND=<a ground tile model>`.
