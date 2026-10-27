# Pakal Engine

Pakal is a modular game engine written in C++20. Rendering goes through bgfx, 2D physics through Box2D, and games are
loaded as modules (a DLL on Windows, linked statically on Android).

## Features

- **Rendering (bgfx)**: Direct3D 11 on Windows, OpenGL ES 3 on Android. Static and skinned glTF/Collada meshes (Assimp),
  metal-rough PBR materials, sun shadows fitted to the camera with simplified shadow casters, a baked ambient light volume,
  frustum culling, a post-process pass (color grading, tilt-shift blur, ambient occlusion, fades), and quality levels
  (low, medium, high, ultra) that cap shadows, post-process samples and render resolution per device.
- **Physics**: Box2D 2D world, character controllers, ray casts, nav grid and an enemy AI controller.
- **Audio**: SFML on desktops, an AAudio mixer on Android.
- **Input**: keyboard and mouse; on Android, two virtual sticks exposed as an axis device.
- **Tooling**: an MCP server in the desktop player (screenshots, FPS, entities), and per-pass performance logs
  (`PAKAL_FPS_LOG=1`).

## Examples

- **Zombies** (`examples/zombies_module`): a top-down shooter. Actors, components and a fixed-step simulation, Lua mission
  scripts, enemy waves, weapons, consumables, destructible props. Its **Downtown** level is built from Quaternius's CC0 packs
  (see [docs/ZOMBIES_DOWNTOWN.md](docs/ZOMBIES_DOWNTOWN.md)). Runs on Windows and Android.
- `basic_example`, `engine_feature_lab`: small engine samples.

The free asset packs are not stored in Git. Download them from Quaternius's site and run
`python tools/fetch_free_assets.py` (see [docs/ASSETS.md](docs/ASSETS.md)). Some data the Zombies levels still use (game
rules, sounds and shot effects) comes from a level converted from an original game, which is not free and not included.

## Quick start

```bash
git clone https://github.com/bacsmar/Pakal.git
cd Pakal
git submodule update --init --recursive
```

Windows (Visual Studio + vcpkg): `cmake --preset win64` then `cmake --build --preset win64-release`.
Android: `python android/build_apk.py` (NDK, SDK and JDK paths in [docs/ANDROID.md](docs/ANDROID.md)).
Details: [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

- [docs/README.md](docs/README.md) - index
- [ARCHITECTURE.md](docs/ARCHITECTURE.md), [SUBSYSTEMS.md](docs/SUBSYSTEMS.md), [BGFX_BACKEND.md](docs/BGFX_BACKEND.md) - the engine
- [ZOMBIES_MODULE.md](docs/ZOMBIES_MODULE.md) - the Zombies example and its debug switches
- [ANDROID.md](docs/ANDROID.md) - Android build and quality levels
- [TOPDOWN_MODULE.md](docs/TOPDOWN_MODULE.md) - what top-down games share
- [LEVEL_BUILDER.md](docs/LEVEL_BUILDER.md), [LEVEL_EDITOR.md](docs/LEVEL_EDITOR.md) - building and editing levels
- [NETWORK.md](docs/NETWORK.md), [TELEMETRY.md](docs/TELEMETRY.md) - network play, run telemetry

## Platforms

- Windows: Direct3D 11 (other bgfx renderers selectable with `PAKAL_BGFX_RENDERER`)
- Android 8.0 or later, arm64: OpenGL ES 3
- Linux: a development Dockerfile (Ubuntu 24.04) is provided; the Zombies example has not been verified there
