# The level editor

A standalone editor for TopDown levels, in the manner of Unity's or Godot's scene view. It runs in PakalPlayer with a
game's module, so it shows the level with that game's models and plays it with that game's own state:

```
build-win64\bin\PakalPlayer.exe ZombiesModule.dll --edit Assets/zombies/<level>/level.json
```

`--edit` without a level opens the editor on its level chooser. **Level > Open** (Ctrl+O) changes level without closing:
type a path (from the game's folder, or a full one), pick one of the levels opened last (`editor_recent.json` next to
the game), or one of every `level.json` under `Assets`, with a filter. A level with unsaved changes is not left without
asking.

`--edit [level]` reaches the module as `PAKAL_EDIT` (`*` without a level); a module that supports it pushes `LevelEditorState`
(`game_genres/topdown/editor`, library `PakalTopDownEditor`) and gives it the state that plays a level.

It edits the level's `level.json` (the `pakal-level` document) through the same operations as `pakal-level` and the
in-game level panel (docs/LEVEL_EDITOR.md). The undo history (`<level>.history/`) is shared with them. A level made by the
level builder is a build output: edit its scene instead when it will be built again (docs/LEVEL_BUILDER.md).

## The window

- **Menu bar**: Level (Save, Reload from disk), Edit (Undo, Redo), the gizmo's mode (Move, Turn, Scale), Local or world
  axes, Snap, **Play**, the level's name (`*` while it has unsaved changes) and the last thing that happened.
- **Level** window:
  - **Objects**: every object by key, with its prefab. Type in the filter to search keys, names, prefabs and zones. A
    click selects, Ctrl or Shift adds to the selection, and a double click looks at the object.
  - **Prefabs**: the level's prefabs and how many objects use each. Click one, then click the floor to place it (Shift
    keeps placing).
  - **Camera**: the level's intro camera, its shots (see "The intro camera" below).
- **Inspector**: the first selected object.
  - Key, name, role and zone.
  - Transform: position in meters, rotation in degrees, scale. Fields apply on Enter.
  - Its prefab:
    - **Apply to prefab**: its overrides go into the prefab's file.
    - **Revert all**.
    - **Make prefab**: a prefab file from the object, which is then made from it.
  - Its components, each field from the component schema (see below). An overridden field shows in blue, with a
    **revert**. **Remove** takes out a component; **Add component** adds one.
- **The view**: the selected objects' boxes are outlined, and the first one has the gizmo.

| Input | Does |
|---|---|
| Left click | Picks the object under the pointer: of the boxes the ray goes through, the one smallest across. Shift adds or removes. A click within 12 pixels of a shot's eye or look point takes that point instead |
| Right drag | Turns the camera around the point it looks at |
| Right button held + W A S D | Flies over the floor (Shift: faster) |
| Middle drag | Pans |
| Wheel | Zooms |
| W / E / R | Move / Turn / Scale gizmo |
| F | Looks at the selection |
| Ctrl while dragging the gizmo | Snap the other way (on when Snap is off) |
| Ctrl+D | Duplicates the selection, 1 m east |
| Delete | Deletes the selection |
| Ctrl+Z / Ctrl+Y / Ctrl+S | Undo / Redo / Save |
| Ctrl+O | Opens another level |
| Esc | Stops a shot looked through or the path playing; else stops placing, clears the selection |
| F5 | Play / Stop |

Each gizmo drag is one undoable `place` operation, made when the gizmo is let go. The matrix is taken apart into the
document's position, rotation (degrees, Rz·Ry·Rx about the level's Z-up axes) and scale (`EditorMath::decompose`).
The gizmo moves the first selected object.

## Layers: a mission over a base level

A big base map is loaded once; each mission is a layer over it that holds only what the mission adds, hides or
changes (the format and its loading: docs/LEVEL_EDITOR.md, "Layers", `tools/level_doc/LevelLayers`). The editor edits a
layer as one level, composed with its base, and saves only the layer (`tools/level_doc/LevelLayerEditing`):

- **Level > New layer over this level...** makes a layer file that extends the level open (by default in a folder next
  to it, `<folder>_layer/level.json`) and opens it. Save the base first: the layer is made over the file.
- The base's objects are dimmed in the view and grey in the list. While **Lock the base** is on (Level menu), a click does
  not pick them; they can still be picked in the list.
- A new object goes into the layer. A base object that is moved, turned, scaled or changed becomes an `adjust` entry
  (only what changed). One that is deleted (or **Hide it in this layer** in the inspector) becomes a `hide` entry.
  **Hidden from the base** in the list puts one back.
- New zones are the layer's. Every other section of the layer file is kept as it was.
- The Level menu lists the keys the layer hides or adjusts that its base no longer has.
- A base whose objects have no keys cannot be layered over: open the base and save it (it gets keys), or run
  `pakal-level <base> keys`.
- The editor reads the level again when its base's file changes outside.
- A base object's collision follows it: hidden, it goes with it; adjusted, it moves, turns and scales with it (see
  docs/LEVEL_EDITOR.md, "Layers").

## The intro camera

A level (or a mission's layer) can give the shots its intro pans through, instead of the game's own pan:
`"camera": {"intro": {"seconds", "end", "shots": [{"eye", "look", "at", "fov"}]}}`, in level units with z up. `eye`
and `look` are points; `at` (a share of the seconds) and `fov` (vertical degrees) are optional: without `at` the shots
are spread evenly, without `fov` the play camera's is used. `end` is `"play"` by default: the play camera is the last
shot, wherever the player starts. `LevelCameraPath` (game_genres/topdown) reads it as a `CameraPan`'s keys, eased as a
whole and smooth through the shots; a game plays those.

The **Camera** tab edits it:

- **Add a shot from the view**: a shot whose eye is where the camera is, looking at the point it turns about. It goes
  after the chosen shot.
- **Seconds**, and **Ends on the play camera** (off writes `"end": "last"`: the path ends on its last shot).
- **Play the path**: the view plays it as the game would. When it ends on play, the last shot is the play camera at rest
  over the spawn, as the default camera rig places it (at the level's `camera.distance`); the game's own rig may differ.
- The chosen shot: its eye and look in meters, **Its own time** (`at`), **Its own field of view** (`fov`), **Look
  through it**, **Set from the view**, **Earlier** / **Later**, **Delete the shot**.
- **Remove the intro camera**.

In the view each shot's eye is a cross with a line to the point it looks at, and the eye's path through the shots is
drawn. A click on an eye or a look point takes it, and the move gizmo moves it. Moving the camera leaves a shot looked
through.

Every change is one undoable `camera` operation (`{"op": "camera", "path": "intro", "data": {...}}`, null data removes
the path; `pakal-level <level> do '<json>'` does it too). In a layer, a camera path that differs from the base's is
saved whole in the layer and replaces the base's; the base's other camera settings (`distance`) stay the base's.

## Working with an agent: live editing

While the editor is open on a level, `pakal-level` on that level works in the editor instead of the file
(`tools/level_doc/LiveEditing`). A person and an agent (or a script) can then work on the same level at once:

- **The operations are done in the open document.** They are undoable with Ctrl+Z like the person's own, and marked
  as the agent's: the history keeps `"by": "agent"`, Edit > Undo says "(the agent's)", and the menu bar shows the last
  one for a few seconds.
- **They wait for the person.** While a gizmo or a field is being dragged or typed in, the requests wait and are done
  when it is let go.
- **They are saved at once when nothing else was unsaved.** Otherwise they stay with the person's changes, unsaved, and
  `pakal-level` says so.
- **A layer is edited composed**, as in the editor: moving a base object becomes an `adjust` entry of the layer.
- **`pakal-level <level> live`** says whether an editor is open on the level and what it shows: the selection (keys,
  places, prefab), the shot chosen, the view (where the camera is and the point it turns about), unsaved changes.
  "Put a barrel by what I selected" needs no coordinates.
- `do` takes a JSON array of operations: done in order, stopping at the first that fails (the ones before it stay done).
- `--file` changes the file even with an editor open. Reading commands (`info`, `list`, `show`) always read the file,
  and warn when the editor has unsaved changes.
- While the editor plays the level, requests are refused: Stop first.

The way is a folder next to the level, `<level>.live/` (ignored by git), which the editor removes when it closes:
`editor.json` is its heartbeat (written every second; older than 4 s means no editor), `inbox/` holds the requests
(each a JSON file written whole) and `outbox/` the answers. A request nobody takes in time is withdrawn, so it is never
done later unseen. Nothing goes over the network.

## Prefabs

A prefab is a file `prefabs/<Name>.prefab.json` next to the level (`meta.prefabs` names another folder, relative to the
level):

```json
{
  "mesh": "../meshes/pr_barrel.dae",
  "role": "prop",
  "tags": ["breakable"],
  "collision": { "shape": "box", "size": [0.8, 0.8] },
  "components": { "Destructible": { "health": 40 }, "Explosive": { "radius": 3 } }
}
```

- `mesh` is relative to the prefab's folder.
- `"base": "Barrel"` makes a variant: a merge over its base, where `null` removes a key.
- A level's object made from a prefab says `"prefab": "Barrel"`. It keeps only what it changes in `"overrides"`, a JSON
  merge patch (RFC 7386) on the prefab, e.g. `{ "components": { "Destructible": { "health": 55 } } }`. Its own `mesh` is
  empty.
- Editing the prefab changes every object made from it; a changed prefab file is read again by the editor and by a
  level that is playing.
- An object without a prefab can carry components in its overrides too.

`tools/level_doc/PrefabLibrary` reads, resolves (prefab, bases, overrides) and writes prefabs. The operations
`instantiate`, `override`, `revert`, `apply-to-prefab` and `make-prefab` are in docs/LEVEL_EDITOR.md. Writing a prefab
file is not part of the level's undo.

## Components and their schema

`game_genres/topdown/ComponentSchema` lists the components a level's object can carry, with their fields (type,
default, range, help) and how each joins the object's actor. The inspector is made from it.

| Component | Fields |
|---|---|
| `Destructible` | `health`, `deathSound`, `drops`, `dropRate`, `randomDrop`, `repairable`, `repairSeconds`, `objective` |
| `Explosive` | `radius`, `damage`, `burningEffect` (read with Destructible) |
| `GiftBox` | `hidden` |

A game adds its own kinds, or replaces one by adding it again: `LevelEditorState`'s third argument, and
`ComponentSchema::add` in the game's level state. An unknown component is kept in the document, shown as JSON in the
inspector, and reported when the level is built.

`LevelObjects` (`game_genres/topdown/view`) builds a level's objects by key:
1. the model, from the prefab or the object's own mesh;
2. for an object with components, a prop actor with them;
3. the game can handle an object itself first (Zombies: occluders, its props by game id).

## Play and live changes

**Play** saves the level and pushes the game's state for it on top of the editor; the editor's models go meanwhile.
**Stop** (the button at the top, or F5) pops it (`GameStateManager::request_pop`), and the editor comes back as it was.

A level played from the editor follows its file. Zombies turns on `ZOMBIES_LIVE_RELOAD` for it. While it plays, a change
saved by `pakal-level`, an agent, the in-game panel or a text editor is applied this way:
- **Only objects changed** (moved, added, removed, a prefab or an override): they are brought in line where they are
  (`LevelObjects::update`). The model, the actor, its prop view and its footprint follow, and the game keeps running.
- **An object the game's own rules made was removed or changed beyond its placement, or anything else changed** (walls,
  scripts, rules): the level is built again, keeping the player where it was.

The editor reads the level again when its file changes outside and it has no unsaved changes. Otherwise it says so, and
saving overwrites the file.

## Checking it

`PAKAL_EDITOR_SELFTEST=1` makes the editor try itself without a mouse:
1. it picks the object drawn nearest the middle of the view, through the point of the screen its middle is drawn at;
2. it moves the object as the gizmo would, then undoes it;
3. another program (a thread, as pakal-level would) moves the selected object through the open editor; the move is
   done, marked as the agent's, answered with what is selected, and undone with the editor's undo;
4. it plays the level and stops;
5. it makes a layer over the level, hides one base object and moves another, saves, checks that the file holds only
   that and loads composed as edited;
6. in that layer it sets an intro camera of two shots, takes the first shot's eye with a click near it, moves it as the
   gizmo would, looks through it, plays the path, saves and checks the layer file holds the camera; then it removes
   the layer.

It logs `[LevelEditor] self test PASS/FAIL` lines and takes `editor_selected.tga`, `editor_play.tga` and
`editor_back.tga`, `editor_layer.tga`, `editor_shot.tga` and `editor_path.tga`. `LevelLayerEditingTests`, `LiveEditingTests`, `PrefabTests` and `ComponentSchemaTests` cover the document's and the schema's side.
