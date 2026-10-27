# Level builder and its procedural tools

`tools/levelbuilder` turns a request ("a small US town with a park and a couple of missions") into playable Zombies
levels, using only the free Quaternius packs (CC0). It is driven from the command line (`lb.py`), so Claude can build and
change levels step by step; `tools/levelbuilder/CLAUDE.md` is the short working guide, this page explains the parts.

    python tools/levelbuilder/lb.py <command> <scene> [options]

## Pipeline

```
packs (assets_zip) --fetch_free_assets--> installed packs --assets_index--> docs/ASSET_INDEX.md
                                                |
                                            catalog.json  (models, sizes, tags, roles)
                                                |
 brief --generate--> scene.json  <--operations (streets, lots, park, houses, wreck, ...)--  Claude
                         |  validate (overlaps, zones, placement rules)
                         |  render (top and perspective views, labels)
                         v
               build: one level per mission  (Assets/zombies/lb_<scene>_<mission>/level.json)
                      baked light shared by the missions, heavy buildings drawn from optimized copies
```

## Assets

- **Packs**: Downtown City, Medieval Village, Stylized Nature, Zombie Apocalypse Kit, Universal Animation Library 1 and 2.
  `python tools/fetch_free_assets.py --downloads <folder with the zips>` installs them (they are not in the repository).
- **Index**: `python tools/assets_index.py` lists every model (family, triangles, size, materials, textures) and every
  texture (kind, size, which models use it, what it is for) in `docs/ASSET_INDEX.md`. Look there before deciding that
  something is missing: a texture of a pack can become a new piece.
- **Catalog** (`catalog.py`): the models the builder places, with their real size, pivot, tags (what collides, what is a
  building, what breaks) and roles ("serves as a library", "serves as a crashed car").
- **Own pieces** (`own_assets.py`, `examples/zombies_module/assets/own`): flat ground tiles of 2 triangles: grass (our own
  texture: no pack has a grass ground texture), dirt (Downtown's `T_Dirt`, with normal and ORM maps), sand
  (Nature's `Rocks_Desert`), a 2 m stone path (Nature's `PathRocks`). Pack textures are read from the installed packs and
  copied next to the model when a level is exported.

## Scenes

A scene (`scenes/<name>/scene.json`) is the source of truth: entities (readable ids, model, position, yaw, scale, zone,
role, layer), zones (named rectangles with a kind: street, block, park, lot, spawn...), missions. Every change saves a
version (`history`, `restore`).

- **Layers**: `base` is the fixed town (the only thing that counts for the baked light, shared by all missions); `props`
  (what breaks on the sidewalks) goes into every mission; any other layer is added by the missions that list it.
- **Collision** comes from the catalog: ground, decals and small plants are walked over, trees block with their trunk,
  the rest with its footprint. The scene bounds are an invisible wall.

## Placement rules (`rules.py`)

`lb.py rules` prints them; `validate` reports what breaks them, so a scene made by hand and a generated one follow the
same rules.

- Which role goes in which zone kind: street lights and hydrants on sidewalks, the town sign at an entrance (never in a
  park), buildings in blocks, the ring or lots, plants and paths in green zones, cars in streets and lots.
- Minimum distances: trees 2.5 m from street lights, hydrants 1.5 m from lights and trees, lights 8 m apart.
- Ground per zone kind: a park lies on grass, a block on asphalt, dirt or grass, a lot on asphalt or dirt.

## Town generator (`generate.py`)

    lb.py generate <scene> --force --brief '{"grid": [3, 3], "arm": 5, "seed": 7,
        "blocks": {"park": 1, "downtown": 1, "shops": 1, "houses": 1, "civic": 0, "parking": 0}}'

From a brief and a seed it builds a whole town with the operations (so it follows the rules):

1. The street grid (crossings, four-lane streets, sidewalks, the exits closed by containers, spawn zones behind them).
2. A kind per block: **park** (grass, stone paths, trees, bushes, flowers, rocks), **downtown** (generated offices of 3
   to 6 stories side by side on every side: a continuous street front), **shops** (a shopping street with a parking lot in the middle), **houses** (generated houses,
   lawns and trees), **civic** (police station, library, school, church), **parking** (cars and containers).
3. The ring around the grid filled with low buildings facing in (houses and offices of 3-4 stories), the sidewalks (street lights, trees between them, hydrants,
   trash), the town sign at an entrance, the water tower and the gift box in the park, a survival mission.

Then `validate`, `render`, and refine by hand (or try another seed). `examples/villa.py` is a generated town with its
missions; `examples/pueblito.py` one built command by command.

**Historic center** (`"layout": "historic"`, `examples/centro.py`): the grid of a Spanish or Mexican colonial town
center, 3 x 3 blocks around the main square:

- the **square** (plaza mayor) in the middle block: paving, a kiosk at the center (Medieval arches under a tiled roof),
  planters around it, a row of trees just inside its edge, street lights in the corners;
- the **church** north of it, facing the square: the nave (8 x 14, its door and gable to the square) and the bell tower
  (4 x 4, four stories, arches on the last one, the tower roof);
- the **government palace** east of it: stone office buildings with mouldings, three stories;
- **arcades** (portales) west of it: houses whose front ground floor is a row of arches with the shop wall 2 m behind;
- in the corner blocks, gardens and a car park so it is not all buildings: `"parks"` (2 by default) blocks of grass,
  paths and trees, `"parking"` (1) a lot with rows of parked cars;
- the other blocks **mixed**: tiled houses of two stories and offices of three or four, taking turns side by side, with a
  grass courtyard and trees inside.

Trees are mixed everywhere (the common ones and some pines: `ops.TREES_MIX`); buildings stay low (3-4 stories).

## Building generator (`buildings.py`)

Houses are composed from the modular pieces of the Medieval Village pack, whose walls are 2 m wide and 3.12 m tall and
whose tiled roofs come in sizes by footprint (4x4 to 8x14 m).

- **Recipe**: footprint (width x depth, the sizes that have a roof), stories, style (plaster, brick, mixed: brick ground
  floor and plaster above), seed, shutters, chimney.
- **Pieces**: every 2 m of every side a wall piece: the door at the front center, windows (with their frames and, on
  plaster, shutters open or closed) or plain walls (half-timbered on plaster); corner pieces; the roof; the brick gables
  under it; a chimney.
- **Merge and optimize** (`blender_batch.py`, run by Blender in the background): the pieces become one model, so the game
  draws a house as one object, not forty. The same pass then joins the repeated materials (every imported piece brings
  its own copy: one draw per texture instead of per piece), dissolves coplanar faces (nothing visible changes) and, if
  the house is still over 9000 triangles (stone walls have every stone modeled), simplifies it down to that. A two-story
  brick house goes from 36 000 triangles and 29 draws to 9000 and 7.
- The models go to `examples/zombies_module/assets/generated/` (not in the repository: made again from their recipe,
  kept next to them as JSON) and into the catalog as `Building_House_<w>x<d>_<stories>_<style>_<hash>`.
- `lb.py`'s `houses` operation (used by the generator for residential blocks) makes a few designs and places them along
  the sides of a block, doors to the street, with yards between them.

**Offices** are composed from Downtown City's modular pieces: walls 2 or 4 m wide and 3 m tall (their outer face at the
pivot, as the Medieval walls), corner columns in sections (base, middle, top, cap), parapets 1 m tall, 4 x 4 m roof slabs.

- **Recipe** (`office_recipe`): footprint in multiples of 4 m (8 to 16), stories, style, seed. Styles: **brick** (trimmed
  windows, arched or double ones, brick corner columns), **trim** (stone with mouldings), **metal** (glass and steel).
- **Pieces**: the ground floor of the front is shop windows with the door in the middle (frame and leaf); the other sides
  alternate shop windows and wall. Upper floors repeat one window pattern per building, in 4 m bays (one 4 m piece or two
  2 m ones); the sides get a blind bay now and then. The parapet runs along every side in 2 m pieces whose ends (left and
  right, seen from outside) turn the corner; the roof is slabs.
- The same merge and optimize pass, with a budget of 20 000 triangles (their windows are finer than a house's): a
  building weighs 11 000 to 20 000, against 18 000 to 45 000 for the pack's whole buildings.
- `offices` places a few designs along the sides of a block almost touching (a 0.4 m gap): the downtown blocks of the
  generator. Models: `Building_Office_<w>x<d>_<stories>_<style>_<hash>`, in the catalog as office, apartment, shop.

**Heavy whole buildings**: Downtown's three complete buildings weigh 18 000 to 45 000 triangles. When a level is built,
the same pass makes optimized copies (`<model>_Opt`, at most 15 000 triangles) and the level draws those, colliding with
the original footprint. The static base can also be merged per 32 m cell (`build --batch`), but at the game's camera that
drew more than separate models (per-model culling already removes most of the town), so it is off.

## Baked light: probes and lightmaps

Two kinds of baked light, both computed once per fixed base and shared by every mission of a scene:

- **Light probes** (`gi.json` / `gi.bin`, `tools/zombies_convert/bake_gi.py`): a 3D grid of ambient light samples over the
  town (dark in alleys, warm near sunlit walls). Everything but the floor reads them: buildings, characters, props that
  break, trees, plants, street furniture.
- **Lightmaps** (`lightmaps.py`, `blender_lightmap.py`): the flat floor of the base (streets, sidewalks, ground tiles,
  road marks) gets its light per texel, baked with Blender's Cycles path tracer (on the GPU when there is one; about 20 s
  for a town):
  1. Every model that receives a lightmap gets a copy with a second UV set (`<model>_LM<version>` in
     `assets/generated/`, made by the same Blender pass as the houses: faces of one plane stay together, nothing overlaps)
     and its surface area.
  2. The atlas (2048 x 2048) is shared out by area: every instance gets a square with the same texels per meter (up to 8),
     packed in shelves. Instances of one model share its mesh; each object of the level says where its square is
     (`"lightmap": [scale, u0, v0]`).
  3. Cycles bakes three passes: the sky's light (direct and bounced), the sun's bounce, and the sun's shadow. Buildings,
     trees and street furniture block light without receiving it (all the floor's receivers are joined into one mesh
     first: Cycles bakes object by object, and with hundreds of tiles the GPU sat idle); a large plane stands for the
     ground outside the town.
  4. The atlas is a PNG: RGB is the square root of half the ambient light (more levels in the dark, where they show), A is
     the sun's visibility. It is scaled so the typical open floor gets the level's ambient color, the same reference as
     the probes. It is cached by a key of the base, the light and the bake script (`Assets/zombies/lightmaps/`).

The game replaces the ambient light of those surfaces with the lightmap, and darkens the sun with its baked shadow where it
is darker than the real-time shadow map (beyond the map's reach, or without shadows). The objects that were part of the
bake are marked `"baked"`: at Low quality the shadow map leaves them out and holds only what moves (characters, props that
break), at half the size. Buildings were tried as receivers and came out patchy: their simplified walls are hundreds of
small UV islands that each take a different value at this density, while the probes light them evenly. Small plants are
walked over too, but they keep the probes. `build --no-lightmap` leaves everything to the probes;
`ZOMBIES_LIGHTMAP=off` ignores the lightmap in the game.

## Missions

Each mission becomes its own level. Modes (from the original game's mission types):

| Mode | Goal | Options |
|---|---|---|
| survival | kill N zombies | `--kills` |
| destroy | break the listed targets (infested cars...) | `--targets` |
| holdout | stay alive until the countdown ends | `--seconds` |
| collect | pick up tokens spread around before the time runs out | `--tokens --seconds --seed` |
| race | reach the checkpoints in order before the time runs out (a beacon on the next one) | `--checkpoints --seconds --seed` |
| rescue | find the keys, then reach the cage (the farthest point from the start) | `--keys [--seconds] --seed` |
| defend | keep the survivors' armored truck alive until the countdown ends (lost if it falls) | `--seconds` |
| crates | break the infected supply crates in order (a beacon on the next one); each one adds time | `--crates --seconds --seed` |
| escort | walk a survivor (`--hero lis\|matt\|shaun`) from the start to the farthest street point | `--hero --checkpoints --seed` |
| explore | no zombies, no goal: walk around (starts in the plaza when there is one) | |

**Defend**: in a large town the mission is fenced like survival and holdout (`ops.action_area`), but each street leaving the
area has a gate in the middle of its container barricade: three traffic barriers (`TrafficBarrier_1`, role `gate`) that
the zombies break (120 health) and the player puts back up by standing next to one (3.5 s from nothing; once half of it
is up it stops bodies again). Zombies come in outside the gates. The truck (`Vehicle_Truck_Armored`, role `objective`,
1500 health) stands on the crossing at the middle of the area; two of every three zombies go for it unless the player is
close to them. Gates and truck are our own props (`export.OWN_PROPS`: `Barricade` and `Objective` components), and they
drop nothing. In the game a gate is a body-only collider (`kBarricadeCategory`): bullets, sight and the walkable grid go
through it.

**Escort**: the survivor (one of the kit's heroes) starts next to the player and walks the route's points along the
streets; it waits while the player is more than 7 m away; the zombies go for whichever of the two is nearer; it has 150
health. Winning the mission rescues that hero in the campaign (`unlocks`): it can be played from then on.

**Each mission's own clutter**: a mission brings a layer of its own, `<id>_props` (made when it is created, unless
`--no-clutter`; the generator's survival mission too): groups of street junk from the Zombie Apocalypse Kit and the other
packs (barricades with cones, trash bags, barrels, broken pallets, a couch, tyres, pipes, crates, a container; a blood
stain under half of them) in free places of streets, squares and lots, and three piles of crashed cars in the streets.
Seeded by the scene and the mission's id: every mission of a town looks different, and the same each time it is built.

**Loot chests**: each mission also gets two of the kit's chests (role `loot`) in free places of its streets, squares and
lots: they break with a couple of shots and always drop health or a boost (`export.DROPS_LOOT`).

**Street furniture** (`ops.sidewalks`): a street light every 18 m, a tree between two of them, hydrants, trash bags, and a
traffic light at the entrance of each crossing (the kit's two models, on the right of who arrives). Poles count as their pole
when things are placed and checked (the arm goes overhead, as their collision in the game); a barricade that has nowhere
else to go clears the trash, traffic lights and hydrants in its way.

**Drops**: what breaks (barrels, trash, cones, cars) drops something more than half of the times: money, health (a red
cross on the floor) or a boost (speed, damage, defence: a diamond in its color); the cars more often, and an air strike
now and then (`export.DROPS`, `DROPS_EXPLOSIVE`). The game applies a dropped health pack or boost as using it does.

Each mission starts at its own spot: a free place on a street, chosen with the scene's name and the mission's id
(always the same for that mission), away from the zombies' entrances and at least 25 m from the starts of the scene's
earlier missions; a mission's `"start": [x, y]` sets it by hand. Tokens, checkpoints and keys keep away from it.

The game shows an arrow by the player toward the next beacon or token. Losing (the time runs out) shows the mission
failed panel. The campaign (`examples/zombies_module/game/campaign.json`) lists which built missions the game offers.

## Building the levels

`lb.py build <scene> [--mission <id>]` writes one level per mission: the objects and their colliders, the breakable props,
the spawn points, the mission script, and the baked light of the fixed base (computed once per base and shared by the
missions). Play one with `ZOMBIES_LEVEL=Assets/zombies/lb_<scene>_<mission>/level.json`, or from the game's menu if the
campaign lists it.
