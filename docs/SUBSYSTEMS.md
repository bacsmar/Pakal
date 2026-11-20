# Pakal subsystems

## GraphicsSystem
Main implementation: `BgfxGraphicsSystem` (files: `source/bgfx/BgfxGraphicsSystem.h`, `source/bgfx/BgfxGraphicsSystem.cpp`). Irrlicht no longer exists in the repo.

Responsibilities:
- Initialize the bgfx context and the selected render backend.
- Handle the render frame cycle.
- Provide 2D and 3D cameras: `CameraComponent_Bgfx`, `CameraComponent3D_Bgfx`.
- Provide sprite components (`SpriteComponent_Bgfx`) and a debug drawer (`BgfxDebugDrawer`).

Notes:
- The Rocket-based UI is not available with the bgfx backend.

## PhysicsSystem
Main implementation: `Box2DPhysicsSystem` (`source/box2D/Box2DPhysicsSystem.h`).

Responsibilities:
- Create and maintain the `b2World`.
- Create/destroy bodies and joints.
- Provide a debug drawer to visualize shapes in `GraphicsSystem`.

Notes:
- Configurable through `PhysicsSystem::Settings` (gravity, iterations, continuous physics, etc.).

## Entity / Component Systems
There are two models (see ARCHITECTURE.md, "Entities: two models").

Asynchronous (`AsyncEntity` / `AsyncComponent`). Relevant files: `source/AsyncEntity.*`, `source/AsyncComponent.*`,
`source/ComponentManager.*`, `source/EntityManager.*`, `source/components/*`.
- `EntityManager` manages the entities (IDs) and their components.
- `ComponentManager` registers `IComponentProvider`s (each System implements a provider) and factories that create concrete components (e.g. `SpriteComponent2D`).
- Components have a lifecycle through `initialize()` and `terminate()`, which return `BasicTaskPtr` (asynchronous task system).

Synchronous (`SyncEntity` / `SyncComponent` in a `SimWorld`): `source/SyncEntity.h`, `source/SimWorld.h`. A game's
simulation in fixed steps; the components run in order, once per step, on one thread.

## ResourceManager &amp; Persistence
- `ResourceManager` (`ResourceMgr`) reads the engine's files. `read_bytes(path)`, `read_text(path)` and `exists(path)`
  find a file as given, then under the root folder, then in the mounted sources, in that order. Any thread may call them.
- Sources are registered by type (`register_source<T>(factory)`) and mounted with `add_source<T>(args...)`. Pakal has:
  - a folder;
  - a zip (`ZipSourceMiniz`, with miniz). Its names are indexed when it is mounted, ignoring case, with `/` as the
    separator. `PAKAL_MOUNT_ZIP=<a.zip>[;<b.zip>...]` mounts zips at start;
  - on Android, the APK's own assets (`AndroidAssetSource`). It answers from the package's `assets.list` without
    calling the asset manager. A path under the root folder is looked up in the APK by what follows the root.
- Bytes read are shared while anyone holds them: a texture or document read again meanwhile costs nothing. Whether a
  path exists is remembered, so a missing file is not searched for again. Mounting or removing a source, a new root,
  or a file written forget those answers.
- What goes through it:
  - models: assimp reads the glTF and its buffers through an IOSystem;
  - textures, shaders and materials;
  - RmlUi's documents, styles, fonts and images;
  - levels and their layers: `LevelDocument::set_file_reader`, so the level library keeps no dependency on Pakal;
  - audio banks and sounds.
- Persistence and serialization are handled through `Archive` and a related project `pakal_persist` (fork at `bacsmar/pakal_persist`).

## OSManager / Input / Sound / UI
- `OSManager` handles windows and events. It can use SFML or native wrappers (Android) depending on defines.
- `IInputManager` is obtained with `os_manager()->get_input_manager()` and is registered as a component provider.
- `SoundManager` has a configurable backend (SFML or another implementation).
- Music: `SoundManager::play_music(path, MusicSettings)`, `stop_music(fadeSeconds)` and `set_music_volume`. The music is an
  OGG Vorbis resource that `MusicMixer` decodes as it plays (stb_vorbis). A new track crossfades with the one playing, and a
  looping track crossfades its last `loopFadeSeconds` into its start. The desktop renders the mixer into an `sf::SoundStream`;
  Android renders it into its AAudio stream. `tests/AudioTests.cpp` checks the loop, the stereo and the fade out on
  `tests/fixtures/music_tone.ogg`.
- `SoundManager::set_paused`: every sound holds while the engine is paused (`Engine::on_pause` / `on_resume`).
- Gamepads: a standard pad (an Xbox's layout) reads as keys: `set_pad_bindings` binds each `PadInput` to a `Key`, so what a
  game reads with `is_key_pressed` a pad gives too. On Android the two sticks are also axis device 0 (axes 0-1 the left one,
  2-3 the right one, as the touch screen's sticks are), and the touch sticks hide while a pad plays, until a finger lands.
- Menus with keys and pads: `IUIManager::navigate(document, NavDirection)` moves the focus to the nearest button that way and
  `activate(document)` clicks the focused one (its `data-event-click` runs, as a tap's would). The buttons are the elements
  whose style says `tab-index: auto`; the focused one has `:focus-visible`. While an element of class `nav-trap` is shown, only
  the buttons inside it are reached. `ui/UiNav.h` (`Pakal::UiNav`) reads the arrows and Return each frame and calls both.
  Once the keys or a pad have moved through a menu, UiNav keeps one button lit in every document it serves
  (`IUIManager::focus_first`: the first, when none of its buttons has the focus), so a pad's A always presses something;
  a finger alone never lights one.
- UI: no active implementation in the bgfx backend. Rocket GUI required Irrlicht and is no longer available.

## Other
- Task system: `BasicTask`, `TaskUtils` for parallel initialization/termination.
- GameStateManager: handles game states and their update.

See the files and classes in the repo for more depth.
