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

- **Menu bar**: Level (Save, Reload from disk), Edit (Undo, Redo, Select the same prefab, Frame the selection), the
  gizmo's mode (Move, Turn, Scale), Local or world axes, Snap (a right click sets its steps: move in meters, turn in
  degrees, scale), View (the grid; hide, show alone, show all), **Play**, the level's name (`*` while it has unsaved
  changes) and the last thing that happened.
- **Level** window:
  - **Objects**: every object by key, with its prefab. Type in the filter to search keys, names, prefabs and zones. A
    click selects, Ctrl or Shift adds to the selection, and a double click looks at the object.
  - **Prefabs**: the level's prefabs and how many objects use each. Click one, then click the view to place it on the
    surface under the pointer (Shift keeps placing).
  - **Camera**: the level's intro camera, its shots (see "The intro camera" below).
- **Inspector**: the selected object; with several, see "Several objects" below.
  - Key, name, role and zone.
  - Transform: position in meters, rotation in degrees, scale. Fields apply on Enter.
  - Its prefab:
    - **Apply to prefab**: its overrides go into the prefab's file.
    - **Revert all**.
    - **Make prefab**: a prefab file from the object, which is then made from it.
  - Its components, each field from the component schema (see below). An overridden field shows in blue, with a
    **revert**. **Remove** takes out a component; **Add component** adds one.
- **Assets**: every model of the catalog, with its picture (see "The asset browser" below).
- **The view**: the selected objects' boxes are outlined, and the first one has the gizmo.

### Several objects

With more than one object selected, the inspector shows what they have in common and sets a field on all of them in
one step (a `batch` operation: one undo):
- **Name, role, zone**, and **position, rotation and scale** one number at a time: a value they share is shown; one they
  differ in shows a dash, and what is typed (Enter) goes to every one of them, the other numbers left as each has them.
- **Prefab**: the prefab they share (or "several"), Apply to prefab and Revert all on each, Select the same prefab.
- **Components** that all of them have, each field once: a checkbox they differ in shows as mixed, a number or a text as
  a dash, a color or vector with "(mixed)" (it shows the first's); a value set becomes an override on each. **Remove**
  takes the component out of all of them; **Add component to all** adds one to those without it.

The gizmo moves, turns and scales the whole selection: the others take the first one's move, and its turn and scale
about their own pivots; letting go is one undo.

| Input | Does |
|---|---|
| Left click | Picks the object under the pointer: of the boxes the ray goes through, the one smallest across. Shift adds or removes. A click within 12 pixels of a shot's eye or look point takes that point instead |
| Left drag | Draws a box: the objects whose middle is drawn in it are selected when it is let go (Shift adds them, Ctrl takes them out) |
| Right drag | Turns the camera around the point it looks at |
| Right button held + W A S D | Flies over the floor (Shift: faster) |
| Middle drag | Pans |
| Wheel | Zooms |
| W / E / R | Move / Turn / Scale gizmo |
| F | Frames the selection: the boxes of all of it in sight |
| Ctrl+Shift+A | Selects every object of the selected one's prefab (of its model, when it has none) |
| Ctrl+P / Ctrl+Shift+P | Hangs the selection from its first object / from nothing |
| H / Shift+H / Alt+H | Hides the selection from the view / shows it alone / shows everything again. Only the view: the level is not changed, and what is hidden is not picked |
| Ctrl while dragging the gizmo | Snap the other way (on when Snap is off) |
| Ctrl+D | Duplicates the selection, 1 m east |
| Ctrl+C / Ctrl+V | Copies the selection / pastes it around the surface under the pointer, in this level or another (see "Copy and paste") |
| Delete | Deletes the selection |
| Ctrl+Z / Ctrl+Y / Ctrl+S | Undo / Redo / Save |
| Ctrl+O | Opens another level |
| Esc | Stops a shot looked through or the path playing; else stops placing; else clears the selection |
| [ / ] | Turns the selection, or what is being placed, by the turn step about its own vertical (Shift: a quarter turn); one undo |
| V held | Vertex snap: the selection's vertex nearest the pointer is marked; drag it onto a vertex of another model (the nearest one drawn within 24 pixels of the pointer; else the surface under it). The whole selection moves; one undo |
| G | Shows or hides the grid |
| F5 | Play / Stop |

Each gizmo drag is one undoable `place` operation, made when the gizmo is let go. The matrix is taken apart into the
document's position, rotation (degrees, Rz·Ry·Rx about the level's Z-up axes) and scale (`EditorMath::decompose`).
The gizmo moves the first selected object.

## The Device window

View > Device measures and tunes a running game through its engine tools (docs/PROFILING.md): the game on this computer
(`local`, the port of its `PAKAL_MCP_PORT`), or an Android device from **adb devices** (Connect sets up `adb forward` of
the port). It is a client of the same tools `tools/pakal_device.py` calls; the requests run on a thread of its own.

- **Graphs**: the seconds received (the last 300): fps, frame p50 and p95, the GPU's time, draws, triangles, memory, the
  thermal status, the GPU's time of each pass, and the costliest sections of the last second. **Record** writes what
  comes in to `editor_cache/device_<time>.ndjson` (the profiler's records: `tools/pakal_prof.py` reads it).
- **Settings**: the quality, the render scale, FSR, the governor (and its status), the preset of the quality, the
  post-process and the shadows: each field applies as it is let go. **Keep as A / B** keeps the settings; **Apply A / B**
  puts one back and measures the frame p50 of the seconds after it settles. **Write back** hands the settings to the
  game's `write_back` tool (a game provides it: the engine does not know its data files); without one,
  `pakal_device.py writeback --map` writes them by a map.

## Copy and paste

Ctrl+C keeps the selection in `editor_clipboard.json` next to the game, so it can be pasted into any level opened
after: each object as the level writes it, its place in meters, its model's path from the game's folder, and its
prefab whole (bases merged in). Ctrl+V (or Edit > Paste, where the camera looks) puts the group's middle on the surface
under the pointer and its lowest on that surface, with new keys free in the level, its paths made the level's, and the
prefabs the level lacks written into its prefabs folder. Hierarchy links among the pasted objects follow their new
keys; links to objects not pasted are let go. The game ids, lightmap places and baked flags are not pasted. One undo.

## Hierarchy

An object hangs from another by its `"parent"` (docs/LEVEL_EDITOR.md, "Names"): its place stays the level's, so the
game needs nothing new.
- **Edit > Hang from the first selected** (Ctrl+P): the other selected objects hang from the first. **Hang from
  nothing** (Ctrl+Shift+P) lets the selection go. One undo each.
- The **Objects** list is a tree when the level has a hierarchy and the filter is empty: each object under the one it
  hangs from (a click selects, the arrow opens).
- The gizmo, a vertex snap and [ ] carry what hangs from what they move, rigidly: children (and theirs) take their
  parent's move, and its turn and scale about the parent's pivot, in the same undoable step. The inspector's fields
  change the object alone.

## Placing aids

- **The grid** lies on the ground (render height 0) around the point the camera looks at: lines at the move snap step
  near, coarser farther away (at most some 80 each way), every tenth line brighter, the axes through the origin
  brightest. View > Grid or G.
- **Snap** steps a gizmo's move, turn and scale, and puts what is placed on the move step across (its height is the
  surface's).
- **[ and ]** turn the selection by the turn step, each object about its own vertical (Shift: 90 degrees).
- **Vertex snap (V)**, as Unity's: while V is held the selection's vertex nearest the pointer is marked; a left drag
  carries it, and the selection with it, onto the vertex of another model drawn nearest the pointer (within 24 pixels),
  or onto the surface under the pointer. Models keep their triangles in the editor, which is what it reads.
- **On the surface**: a model from the asset browser or a prefab of the Prefabs tab lands on the surface under the
  pointer (a roof, a floor tile, a table; else the ground), upright or standing on the surface's slope (Assets >
  Placing...).

## The asset browser

The **Assets** window shows the models a level can be made of (`game_genres/topdown/editor/AssetCatalog`,
`AssetBrowser`), from what the tools already keep:

- the level builder's catalog, `tools/levelbuilder/catalog.json` (found from the game's folder up; made by
  `tools/levelbuilder/catalog.py`): the free packs' models with their pack, size, roles and tags;
- the manifest of every pack extracted under `Assets/<folder>/installed.json` (`{"<pack>": {"models": {"<model>": ...}}}`,
  each model at `Assets/<folder>/<pack>/models/<model>.gltf`, its colliders in `<model>.colliders.json` when the pack
  has them); a model's roles come from its name (`SM_Bld_...`: building, `SM_Prop_...`: prop, `SM_Veh_...`: vehicle...);
- what `editor_catalog.json` next to the game adds: `{"catalogs": [...], "manifests": [...], "folders": [{"folder",
  "pack", "roles"}], "roles": [{"match": "<regex>", "roles": [...]}]}`.

The search box matches every word against the name, pack, roles and tags; the two lists filter by pack and by role. A
tile's picture is drawn by the engine the first time it is shown (the model alone, framed from the south-east and
above, `BgfxGraphicsSystem::request_model_picture`) and kept in `editor_cache/thumbnails/<pack>/<model>.png` next to the
game; it is drawn again when the model's file is newer.

**Drag a tile into the view** to place its model: while it is dragged, the model is shown where it would land, on the
surface under the pointer (the nearest model's triangles, a floor tile or a roof; else the ground), on the snap grid
across when Snap is on. **[** and **]** turn it. Or click a tile, then click the view (Shift keeps placing, Esc stops).
**Placing...** chooses:
- **Stand on the surface's slope**: the model is tilted to the surface's normal (off: it stands upright).
- **Where a model's prefab goes.** What is placed is an object made from a prefab of its own, written the first time
  the model is used: `{"mesh", "role", "tags", "catalog": "<pack>/<model>", "collision"}`. Its role is its first tag (else
  its first role); `collision` holds the pack's colliders (`{"source": "prefab", "shapes": [...]}`, in the model's axes)
  when it has them. The prefab is named after the model (`<model>_<pack>` when a prefab of that name comes from
  elsewhere). It goes:
  - **in the level's prefabs folder** (the default): the level stays whole by itself, and every game loads it as before;
  - **in a shared library** (a `prefabs` folder beside the level's own folder, or the folder typed): one prefab for
    every level of the game, changed once for all of them. The level then names the library in
    `"meta": {"prefabLibraries": ["../prefabs"]}` (the `prefab-library` operation, in the same undo step as the
    placement), and `PrefabLibrary` looks there after the level's own folder, in the game too.
  The choice is kept in `editor_settings.json` next to the game.

A model's file is named where the catalog has it (the free packs' sources, `Assets/<folder>/...`): a level made this way
is played on the desktop as it is; the level builder's export packs models next to a level for other platforms.
Models keep their triangles in the editor (`MeshComponent3D_Bgfx::set_keep_pick_geometry`), which is what a drop aims at.

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
- **Shared libraries**: `"meta": {"prefabLibraries": ["../prefabs"]}` names folders (relative to the level) looked in,
  in order, for a prefab the level's own folder does not have. A level without the key has only its own folder.
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
   the layer and opens the level again;
7. the asset browser: it has the catalog's models, draws (or reads) the first pictures and keeps one on the disk; a small
   model is dropped on the ground west of everything (made from a prefab written for it), another on top of it, and one
   over the tallest model (on its roof); then they are undone and the prefab removed;
8. placing aids: the grid is drawn, ] turns the selection by the step, a rotation made to stand on a slope has its up
   on the slope's normal, a vertex snap aims at the vertex drawn under the pointer, takes the selection's vertex nearest
   the pointer, and puts it on the other model's top vertex (undone after);
9. selecting: three objects of one prefab are made in one step, "the same prefab" selects them, F frames them (each
   drawn in the view), a box around them selects them, H hides them, Shift+H shows them alone, Alt+H shows all;
10. several objects: the gizmo let go moves all three (one undo), a turn of the first turns each about its own pivot,
   and a component's field set on the three is one step;
11. the engine's tools (docs/PROFILING.md), called from another thread as a client's requests are (in the process, no
   socket: McpServerTests covers it): get_settings, set_settings changes the post-process, set_camera holds and lets go,
   get_stats and get_profile give the live records;
12. the Device window over them: it receives records and settings, a setting changed applies, A/B puts a snapshot back,
   and it records NDJSON;
13. a hierarchy: one object hung from another (Ctrl+P), the gizmo carries it, a quarter turn of the parent takes it round
   the parent's pivot, turned with it, and deleting the parent lets it go;
14. copy and paste between levels: two objects, one hung from the other, copied, pasted into a new empty level with
   their places to each other, the link and their prefab (written there, its model loading); the level is removed after.

It logs `[LevelEditor] self test PASS/FAIL` lines and takes `editor_selected.tga`, `editor_play.tga` and
`editor_back.tga`, `editor_layer.tga`, `editor_shot.tga`, `editor_path.tga`, `editor_assets.tga`, `editor_drop.tga`,
`editor_snap.tga`, `editor_select.tga`, `editor_many.tga`, `editor_device.tga` and `editor_tree.tga`.
`PAKAL_EDITOR_SELFTEST_EXIT=1` ends the game when the test is done. `LevelLayerEditingTests`, `LiveEditingTests`, `PrefabTests` and `ComponentSchemaTests` cover the document's and the schema's side.
