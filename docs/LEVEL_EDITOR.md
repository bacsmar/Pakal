# Editing levels: pakal-level and the level panel

A level (`level.json`, the `pakal-level` format of `tools/level_doc/LevelDocument`) can be changed by hand, by an agent
or from inside the running game. All three go through the same operations (`tools/level_doc/LevelEditor`), so a change is
made the same way whoever makes it, and one undo history serves all of them. To build a new town from the free packs,
use the level builder instead (docs/LEVEL_BUILDER.md); this is for changing a level that exists, whatever made it.
The standalone editor (a free camera, gizmos, an inspector, prefabs, Play) is docs/EDITOR.md.

## Names: keys, roles, zones

- **Key**: every object has a readable key that is unique in its level (`traffic_cone_03`, `mailbox_03`). `pakal-level
  <level> keys` gives one to every object without it, from its name (or its model) and a number. An object can also be
  named by `#<index>` or `id:<game id>` (the id the level's scripts use).
- **Role**: free text a tool filters by (`building`, `prop`, `light`).
- **Zone**: a named rectangle with a kind (`street`, `park`, `spawn`). An object says which zone it is in; `zoning` fills
  that from where each object stands.

Positions in operations and in what pakal-level prints are meters (x east, y north; the file keeps the level's units).
Yaw is in degrees about the vertical axis.

## pakal-level

Built with the engine as `build-win64\bin\pakal-level.exe`. Every change is saved at once, and can be undone.

```
pakal-level <level.json> info                      name, size, spawn, counts, zones, parts, history
pakal-level <level.json> list --near X,Y,R         objects within R meters (also --zone, --role, --match, --limit)
pakal-level <level.json> show mailbox_03           one object in full, with its collision
pakal-level <level.json> move mailbox_03 -13.5 34.5    (its collision goes with it)
pakal-level <level.json> nudge mailbox_03 1 0
pakal-level <level.json> turn mailbox_03 90
pakal-level <level.json> add meshes/pr_mailbox_03.dae -10 30 role=prop
pakal-level <level.json> copy mailbox_03 -9 30
pakal-level <level.json> delete mailbox_03         (and its collision)
pakal-level <level.json> set mailbox_03 role prop
pakal-level <level.json> zone plaza park -24 30 -10 40
pakal-level <level.json> zoning
pakal-level <level.json> do '{"op": "place", "object": "mailbox_03", "z": 0.5, "rot": [0, 0, 45], "scale": 2}'
pakal-level <level.json> do '{"op": "camera", "data": {"seconds": 8, "shots": [{"eye": [0, -900, 700], "look": [0, 0, 0]}]}}'
                                                   the intro camera (docs/EDITOR.md); "data": null removes it
pakal-level <level.json> prefabs                   the level's prefabs and how many objects use each
pakal-level <level.json> prefab Barrel             a prefab with its bases merged in
pakal-level <level.json> instantiate Barrel -10 30 yaw=90
pakal-level <level.json> override barrel_01 /components/Destructible/health 55
pakal-level <level.json> revert barrel_01 [/components/Destructible/health]
pakal-level <level.json> apply-to-prefab barrel_01 its overrides into the prefab's file
pakal-level <level.json> make-prefab mailbox_03 Mailbox
pakal-level <level.json> undo | redo | history
pakal-level <level.json> validate                  exit 1 when the level has an error
pakal-level <level.json> split [section...]        keep sections in files of their own; join puts them back
```

Add `--json` to `info`, `list`, `show`, `zones` and `history` for output a program reads. `pakal-level <level> ops` lists
the operations and their fields.

- **The editor open on the level**: changes and undo go to it instead of the file (its undo, saved when it had nothing
  else unsaved), and `pakal-level <level> live` tells what it shows (the selection, the view). `--file` changes the
  file anyway. See docs/EDITOR.md, "Working with an agent: live editing".

- **Undo**: the history (the last 30 states) is kept in `<level>.history/` next to the level, so it lasts between runs
  and is shared with the game's panel.
- **Prefabs**: an object made from a prefab names it and keeps only its overrides; `show` prints it resolved. See
  docs/EDITOR.md for the prefab file.
- **Game ids**: an object's `id` is kept as written, the converter's negative ones too (only -1, none, is left out).
- **Split document**: `split` keeps the big sections (`objects`, `collision`, `script` by default) in files of their own
  (`level.objects.json`...), which the level names under `parts`. The game and the tools read it back whole; an agent
  reads only the part it needs.

## In the game: live reload and the level panel (Zombies)

| Variable | Effect |
|---|---|
| `ZOMBIES_LIVE_RELOAD=1` | The level follows its file (pakal-level, an agent, a text editor): changed objects are brought in line where they are, anything else builds the level again |
| `ZOMBIES_EDIT=1` | Starts with the level panel open and editing on |
| `ZOMBIES_EDIT_SELECT=<key>` | Selects an object from the start (its gizmo shows in captures) |

A reload keeps the player where it was, and the editor as it was (selection, camera). The panel is the dev panel's
(F1) **Edit level** section (TopDown's `game_genres/topdown/view/LevelEditPanel`):
- While **Editing** is on, the game waits and WASD moves the camera.
- A click on the floor picks the nearest object; dragging the selected one moves it.
- The selected object shows a yellow ring, its east (red) and north (green) axes, and the zones as light blue rectangles.
- Its position, yaw and scale fields move it as they are dragged. Letting go makes one undoable operation.
- **Undo**, **Redo**, **Save**, and **Save and reload** (the collision follows the objects after a reload). Copying or
  deleting an object saves and reloads at once.
- **Live reload** is the same switch as `ZOMBIES_LIVE_RELOAD`.

For an agent: start the game with `ZOMBIES_LEVEL=<level.json> ZOMBIES_LIVE_RELOAD=1`, change the level with pakal-level,
and look with `ZOMBIES_SCREENSHOT` (docs/ZOMBIES_MODULE.md) or by asking the person playing.

## Layers: a mission over a base level

A big base level (a town) is often shared by several missions. Each mission is a **layer**: a `level.json` that names its
base and holds only what the mission changes (`tools/level_doc/LevelLayers`):

```json
{"format": "pakal-level", "version": 2, "name": "town_roadblock",
 "extends": "../level.json",
 "hide": ["car_49", "car_53", "id:1004"],
 "adjust": {"bench_12": {"x": 1250, "rot": [0, 0, 90]}},
 "objects": [{"key": "roadblock_barrier_00", "mesh": "../../models/barrier.gltf", "x": 100, "y": 200, "rot": [0, 0, 45]}],
 "collision": {"segments": [{"layer": "solid", "filled": true, "of": "roadblock_barrier_00", "points": [[...]]}]},
 "script": {...}, "rules": {...}, "lighting": {"sunColor": [1.2, 0.9, 0.7]}}
```

Loading a layer (`load_level`) composes it with its base:
- **Objects**: the base's, minus those named in `hide` (by key, or `id:<game id>`), with `adjust` merged into
  them; then the layer's own.
- **Collision**: a collision segment's `of` names the object it belongs to, so a hidden object takes its collision
  with it. A hidden breakable object takes its game id's walls and destructible too. An adjusted object's collision
  follows it. Its segments (by `of`), its walls and its destructible (by its game id) take the same move, turn and
  scale about its old place. A wall's box turns with it (its `angle`); one that ends square to the axes has no
  angle, and an odd quarter turn swaps its sides. The layer's own segments are
  added.
- **Walls, zones, enemies, pickups, destructibles**: the base's and the layer's together.
- **Missing keys**: a key in `hide` or `adjust` that the base does not have (say, the base was made again with other
  keys) is not an error. `LevelLayering::missing` and `meta.layers.missing` list it, and the game warns.
- **Every other section** (rules, script, spawners, lighting, post, visuals...): the layer's over the base's. Objects
  are merged key by key (a JSON merge patch); anything else is replaced.
- **Paths**: the base's file paths are rewritten relative to the layer's folder.
- **Chains**: a base can extend another base.
- **Where the base is**: the composed document records it in `meta.layers.base`. What lives next to the base, such as
  its baked light (`gi.json`) or its sounds (`audio.json`), is read from there.

The level builder's exporter writes every object's key (its scene entity's id) and every collider's `of`, so a layer
can name them.

In the game, a layer loads composed. Going from one mission to the next, by the next-mission button or through the
menu, keeps the previous level's models: on the same base, only the layer's own models load. Unused models are let go
once the new level is in. The level panel does not save a composed level over the layer's file. The level editor
edits a layer composed with its base and saves only the layer (docs/EDITOR.md, "Layers";
`tools/level_doc/LevelLayerEditing`): a new object goes into the layer's objects, a moved base object into "adjust",
and a deleted one into "hide".
