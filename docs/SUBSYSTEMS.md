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
- UI: no active implementation in the bgfx backend. Rocket GUI required Irrlicht and is no longer available.

## Other
- Task system: `BasicTask`, `TaskUtils` for parallel initialization/termination.
- GameStateManager: handles game states and their update.

See the files and classes in the repo for more depth.
