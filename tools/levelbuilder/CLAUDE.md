# Level builder

Builds playable Pakal (Zombies) levels from the free Quaternius packs, driven from the command line, so that Claude can
turn a request like "a small US town with a park" into a level. Read this before working on a scene.

Run from the repository root with the Python that has numpy and Pillow:

    python tools/levelbuilder/lb.py <command> <scene> [options]

Packs: install them with `python tools/fetch_free_assets.py` (Downtown City, Medieval Village, Zombie Apocalypse Kit,
Universal Animation Library 1 and 2, Stylized Nature). **Every file of them (models and textures, what each is and what
uses it) is in `docs/ASSET_INDEX.md`** (`python tools/assets_index.py` makes it again): look there before saying something
is missing; a texture of a pack can become a piece of our own. Blender (5.x) renders the views and writes the `.glb`; `PAKAL_BLENDER` overrides its
path. Building a playable level also needs the converted Survival level (see docs/ZOMBIES_DOWNTOWN.md).

## Files

| File | What it is |
|---|---|
| `catalog.py` | Scans the packs into `catalog.json`: per model its pack, path, real size, pivot, tags and roles ("serves as") |
| `scene.py` | The scene, `scenes/<name>/scene.json`: the source of truth. Every change saves `versions/scene_vNNN.json` and a line in `history.json` |
| `ops.py` | Operations: streets, lots, fill, scatter, row, wreck, sidewalks, park, town sign, floor, replace, move, validate |
| `rules.py` | Placement rules: which role goes in which zone kind, minimum distances, the ground of each zone kind. `validate` reports what breaks them (`regla:` lines) |
| `generate.py` | The procedural generator: a whole town from a brief and a seed, built with the operations (so it follows the rules) |
| `buildings.py` | The building generator: houses from the Medieval Village pieces (recipe -> pieces -> one merged, optimized model in `assets/generated`), and optimized copies of heavy buildings |
| `own_assets.py` | Our own flat pieces (`examples/zombies_module/assets/own`): ground tiles of grass (our texture), dirt (T_Dirt), sand (Rocks_Desert), 2 m stone path (PathRocks); the pack textures are read from the installed packs |
| `missions.py` | The mission scripts (one level per mission) |
| `export.py` | Scene to playable level (`build_downtown.build_level` without the fixed Downtown city) and to `.glb` plus views |
| `blender_scene.py` | Runs inside Blender: assembles the scene, writes the `.glb` (node names = entity ids) and the renders |
| `blender_batch.py` | Runs inside Blender: merges the fixed models of the base into one model per 32 m cell (`build --batch`) |
| `lb.py` | The command line |
| `geom.py` | Geometry on shapely 2: smooth curves, 1D noise, curve offsets, triangulation, convex pieces |
| `streetnet.py` | A coastal street network from curves (coast, promenade, avenues, fanning cross streets, river and bridges, hill, beach, pier); a PIL preview |
| `roadmesh.py` | Our glTF meshes in 32 m cells: asphalt, curbs, sidewalks, cobblestone, markings, zebras, parapets, decals |
| `terrain.py` | Sea and seawall, river and banks, beach, pier, the hill heightfield, the backdrop |
| `parcels.py` | Buildings along each side of a polygonal block, facing the street |
| `spawners.py` | Spawners as data: candidates, masks, variation, occupancy (presets in `presets/`) |
| `decals.py` | Our wear decal atlas and beach sand texture |
| `theme.py` | Themes: models per role from other packs, extracted under `Assets/` |
| `plazas.py` | Mexican plazas: walks to the center, raised garden beds, kinds (civic, children, dance) |
| `dress.py` | What each lot brings with its building: facade, sidewalk, curb, yard and roof pieces (the preset's `lots`) |
| `setpieces.py` | Scenes told in one place: barricades, crashes, checkpoints, looted shops (the preset's `setpieces`) |
| `overhead.py` | Power poles and cables along the streets, papel picado across them (the preset's `overhead`) |
| `coastal.py` | The coastal generator end to end (`"layout": "coastal"`): block types, walls on open frontage, background ring |
| `examples/pueblito.py` | A complete example town, from scratch |

## Scene model

- Units are meters; level axes: x east, y north. `yaw` in degrees, counterclockwise seen from above; at 0 a model's front
  faces south.
- **Entities**: `id` (readable: `library_01`, `crashed_car_03`), `asset` (a catalog id), `x`, `y`, `z`, `yaw`, `scale`,
  `zone`, `role`, `tags`, `layer`.
- **Zones**: named rectangles with a kind: `street`, `block`, `park`, `spawn` (zombies come in there), ... `streets` creates
  `block_i_j`, `street_h<j>`, `street_v<i>`, the border strips `ring_n/s/e/w` and the spawn zones at the street exits.
- **Layers**: `base` is the fixed town (streets, buildings, street trees, the park). Only it counts for the baked light,
  which is baked once and shared by all the missions of the scene. `props` (breakable things on the sidewalks) goes into
  every mission. Any other layer is additive: a mission lists the layers it takes (`wrecks` for a destroy mission, `fort`
  for a holdout). Put things with `--layer <name>`.
- **Missions**: each one becomes its own level, `Assets/zombies/lb_<scene>_<mission>/level.json`. Modes: `survival` (kill
  N), `destroy` (break the listed target entities), `holdout` (a countdown), `collect` (`--tokens N --seconds T --seed S`:
  tokens spread over streets and green zones; the mission fails when the time runs out), `race` (`--checkpoints N
  --seconds T --seed S`: checkpoints along the streets in route order, a beacon on the next one), `rescue` (`--keys N
  [--seconds T] --seed S`: keys spread around, then reach the cage, the spot farthest from the start), `defend`
  (`--seconds T`: the armored truck on the area's middle crossing, gates the zombies break and the player repairs),
  `crates` (`--crates N --seconds T --seed S`: break them in order, each one adds time), `escort` (`--hero lis|matt|shaun
  --checkpoints N --seed S`: walk the survivor out; winning it unlocks that hero), `explore` (no zombies). See
  docs/LEVEL_BUILDER.md.
- What collides is decided by the catalog tags: ground pieces, decals, paths and small plants are walked over; trees block
  with their trunk; the rest blocks with its footprint. Buildings and trees fade when they hide the player. The scene
  bounds are an invisible wall.

## Rules

`lb.py rules` prints them. In short: street furniture (lights, hydrants) only on sidewalks; the town sign at an entrance
(`lb.py sign`), never in a park; buildings in blocks, the ring or lots; plants, rocks and paths in green zones; cars in
streets and lots. Trees keep 2.5 m from street lights, hydrants 1.5 m from lights and trees. A park lies on grass
(`Ground_Grass_9x9`), not asphalt (`lb.py floor <scene> --zone <z> --asset <tile>` changes a zone's ground). When a request
needs something the rules forbid, change the zone's kind or the rule in `rules.py`, and say so.

## Generator

See docs/LEVEL_BUILDER.md for the whole picture.

    lb.py generate <scene> [--force] --brief '{"grid": [3, 3], "arm": 5, "seed": 7,
        "blocks": {"park": 1, "downtown": 1, "shops": 1, "houses": 1, "civic": 0, "parking": 0}}'

Block kinds: park, downtown (large and medium buildings), shops (a shopping street with a parking lot in the middle),
houses (generated houses from the Medieval Village pieces, lawns), civic (police station, library, school, church),
parking. It also fills the ring,
the sidewalks, puts the town sign at an entrance, the water tower and the gift in the park, and a survival mission. Then
validate, render, and refine with the operations (or another seed). Houses come from `buildings.py`
(`ops.houses`); Downtown offers only three whole buildings, so a downtown block looks alike until office buildings are
composed from its modular pieces too.

Coastal towns (`"layout": "coastal"`, docs/LEVEL_BUILDER.md): `lb.py generate <scene> --brief-file <brief.json>`;
the scene keeps its network, zone polygons, generated meshes (`Assets/levelgen/<scene>`) and collision chains
(`outlines`). `python -m unittest discover tests/levelbuilder` runs the generator's tests. Needs `pip install shapely`.
One block again, the rest as it is: `block_overrides` in the brief, then `lb.py generate <scene> --brief-file <brief>
--force --only-block <id>`; `lb.py build --no-gi` for a quick look (the old light probes stay), a full build to bake.

## Workflow

0. For a new town, start with `lb.py generate` and refine; the steps below build one by hand.
1. `lb.py catalog` (summary and gaps for a town), `lb.py catalog --role tree` (find models).
2. `lb.py new <scene>`, then `streets <scene> --grid 3 3 --arm 5`.
3. Buildings: `lot` (one on a side of a zone, front facing out) and `fill` (a whole side). `park`, `sidewalks`.
4. Mission layers: `wreck ... --layer wrecks`, `row ... --layer fort`. Missions: `mission <scene> <id> --mode ... --layers ...`.
   Every mission gets its own layer `<id>_props` on its own (`ops.mission_clutter`, unless `--no-clutter`): groups of
   street junk (barricades with cones, trash, barrels, junk, crates, tyres, containers, blood on the floor) and 3 crashed
   car piles, seeded by the mission's id, so each mission looks different and has cover. `clutter <scene> --layer L
   --groups N --wrecks N --seed S` makes one by hand; `delete <scene> --layer L` removes one. What breaks drops money,
   health or a boost (`export.DROPS`; cars more).
5. `validate`, then `render <scene> --labels role [--mission <id>]` and look at `scenes/<scene>/renders/`.
6. `build <scene>`: the playable levels. `--batch` merges the fixed base into one model per 32 m cell; measured on the
   example town it made more draw calls (116 against 90) and more triangles, because the game camera sees a small area and
   per-model culling already removes most of the town, so it is off by default. Play one with `ZOMBIES_LEVEL=Assets/zombies/lb_<scene>_<mission>/level.json`.
7. `history <scene>` and `restore <scene> <version>` to go back.

Notes for Claude: keep entity ids readable and stable (replace keeps them). Prefer `lot`/`fill` for buildings, and
`scatter`/`row` with a seed for repeatable results. After changes, validate and render before building. Things that are not
in the packs (benches, mailboxes, gas pumps) are listed by `catalog` with an alternative; say so to the user instead of
inventing models.
