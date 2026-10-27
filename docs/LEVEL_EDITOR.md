# Editing levels: pakal-level and the level panel

A level (`level.json`, the `pakal-level` format of `tools/level_doc/LevelDocument`) can be changed by hand, by an agent
or from inside the running game. All three go through the same operations (`tools/level_doc/LevelEditor`), so a change is
made the same way whoever makes it, and one undo history serves all of them. To build a new town from the free packs,
use the level builder instead (docs/LEVEL_BUILDER.md); this is for changing a level that exists, whatever made it.

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
pakal-level <level.json> do '{"op": "move", "object": "mailbox_03", "dx": 2, "dy": 0}'
pakal-level <level.json> undo | redo | history
pakal-level <level.json> validate                  exit 1 when the level has an error
pakal-level <level.json> split [section...]        keep sections in files of their own; join puts them back
```

Add `--json` to `info`, `list`, `show`, `zones` and `history` for output a program reads. `pakal-level <level> ops` lists
the operations and their fields.

- **Undo**: the history (the last 30 states) is kept in `<level>.history/` next to the level, so it lasts between runs
  and is shared with the game's panel.
- **Split document**: `split` keeps the big sections (`objects`, `collision`, `script` by default) in files of their own
  (`level.objects.json`...), which the level names under `parts`. The game and the tools read it back whole; an agent
  reads only the part it needs.

## In the game: live reload and the level panel (Zombies)

| Variable | Effect |
|---|---|
| `ZOMBIES_LIVE_RELOAD=1` | The level is built again when its file changes (pakal-level, an agent, a text editor) |
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
