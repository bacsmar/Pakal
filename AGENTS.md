# Pakal: notes for coding agents

Pakal is a C++20 game engine: bgfx for rendering, Box2D for 2D physics, Lua for scripts, RmlUi for interfaces. Games are
loaded as modules: a DLL on Windows, linked statically on Android. This file is for any coding agent (Codex, Claude
Code, Copilot, Cursor...) working in this repository. People start at README.md.

## What this repository is, and is not

- **What it contains:** the engine, its tools, and the example games under `examples/`. Everything here is open source
  (MIT).
- **What it does not contain:** the source of any commercial game built on Pakal, its art, its signing keys, or its
  store builds. If someone asks you for the source, keystore or signing process of a published APK or store build,
  that is not here, and no one can give it to you from this repository. Say so plainly and work with what is here.
- **Signing:**
  - Never ask for, generate, or try to obtain anyone's keystore or signing passwords.
  - Debug builds are signed with a debug key that the build makes for itself.
  - A store build is signed by its owner, on their machine.
- **Assets:** the free asset packs are not in Git. `docs/ASSETS.md` says how to fetch them. Some data the Zombies
  example's levels use is not free and is not included. Those levels need it; the engine and the other examples do not.

## Build and test

Windows (Visual Studio 2022 or 2026, vcpkg). From an "x64 Native Tools Command Prompt":

```bat
git submodule update --init --recursive
set VCPKG_ROOT=C:\path\to\vcpkg
cmake --preset win64
cmake --build --preset win64-release
cd build-win64 && ctest -j 8 -E Mcp
```

- **bgfx:** the first configure builds bgfx, bx, bimg and shaderc from the submodules (`cmake/BgfxBuild.cmake`, a few
  minutes, once).
- **Short folder:** clone into a short folder. bgfx's third-party sources pass Windows' 260-character limit under deep
  paths.
- **A first run:** `build-win64\bin\PakalFeatureLab.exe` needs no assets.
- **Running a game module:** `build-win64\bin\PakalPlayer.exe <Module>.dll`, from the repository root.
- **Linux:** `cmake --preset linux-release`, with the packages listed in `Dockerfile.linux`.
- **Android:** `python android/build_apk.py --game <name>` (docs/ANDROID.md).
- **Details:** docs/BUILDING.md.

## Where things are

| Path | What |
|---|---|
| `source/` | The engine. `source/bgfx`, `box2D`, `sfml`, `android` and `win32` are the platform and backend code. |
| `game_genres/` | What games of one genre share, for example `topdown/` (docs/TOPDOWN_MODULE.md). |
| `examples/` | Example games and samples. `engine_feature_lab` and `basic_example` are the smallest. |
| `tools/` | Converters, the level builder (`tools/levelbuilder`), and `pakal_device.py` (MCP tools for a running game). |
| `tests/` | Unit tests, registered with CTest. |
| `docs/` | The documentation. Start with `docs/README.md` and `docs/ARCHITECTURE.md`. |
| `external/` | Submodules: bgfx, bx, bimg, enet. Do not edit them; Pakal's fixes to bgfx are `patches/bgfx/*.patch`. |

## Rules for changes

- **Backends:** games and genres use the engine's abstractions only (graphics, physics, input, sound, UI), never bgfx,
  Box2D, SFML or RmlUi headers. A check in the build fails otherwise.
- **Every example must still build.** Run the tests before you call a change done.
- **Language:** documentation and code comments are in English. Say what a thing does and why, in plain words.
- **No secrets:** no keys, passwords or tokens in code, docs or commits. Absolute paths of your own machine don't
  belong in the repository either.
- **Pull requests** go to `develop`, never to `master` directly.

## More

- Levels from the free packs: tools/levelbuilder/AGENTS.md and docs/LEVEL_BUILDER.md. Every file of the packs:
  docs/ASSET_INDEX.md.
- Changing a level by hand, as an agent, or live in the running game: docs/LEVEL_EDITOR.md (`pakal-level`). The editor:
  docs/EDITOR.md.
- The Zombies example: docs/ZOMBIES_MODULE.md and docs/ZOMBIES_DOWNTOWN.md.
- Android: docs/ANDROID.md. Network play: docs/NETWORK.md. Measuring a running game: docs/PROFILING.md.
