# Starting a new game on Pakal (for agents)

Read this file whole before doing anything. Then follow the steps in order. Every command and file below is complete:
copy them, don't invent your own.

## What you are building

A game is its own git repository. The engine sits inside it as a submodule at `extern/Pakal`, and you never edit it.
The game is a **module**, a DLL that the engine's `PakalPlayer.exe` loads:

```
PakalPlayer.exe MyGameModule.dll
```

Final layout of the game's repository:

```
my-game/
  extern/Pakal/          the engine (git submodule, never edited)
  source/                the game's C++ (the module and its states)
  data/                  the game's files, copied to bin/Assets/<game>/ at every build
  CMakeLists.txt         adds the engine and builds the module
  CMakePresets.json      the vcpkg toolchain, pointing at the engine's manifest
  .gitignore             build-*/
```

## What you need first (Windows)

- Visual Studio 2022 or newer with C++, CMake 3.21+, Ninja and Git. All come with Visual Studio.
- vcpkg cloned somewhere, with `VCPKG_ROOT` pointing to it.
- A Developer shell, so that `cl` works. Every build command below runs in it.

If one is missing, stop and tell the person. Don't install things on your own.

## Steps

1. **Create the repository and add the engine.**

   ```
   mkdir my-game && cd my-game
   git init
   git submodule add <the engine's url> extern/Pakal
   git -C extern/Pakal checkout <a tag or commit you were given; else leave the default branch>
   git submodule update --init --recursive
   ```

2. **Copy the starting point** from the engine's example:

   ```
   cp -r extern/Pakal/examples/game_repository/. .
   ```

   You now have `CMakeLists.txt`, `CMakePresets.json`, `.gitignore`, `source/ExampleModule.cpp` and
   `data/hello.txt`.

3. **Rename** `Example` to your game's name in three places. Use one CamelCase name everywhere (here `MyGame`) and a
   lowercase folder (here `mygame`):
   - `source/ExampleModule.cpp` becomes `source/MyGameModule.cpp`. Inside it, the module name string becomes
     `"MyGameModule"` and the text becomes `"My game"`.
   - In `CMakeLists.txt`: `project(MyGame ...)`, `pakal_add_game_module(MyGameModule SOURCES source/MyGameModule.cpp ...
     DATA data mygame)`, and `add_dependencies(MyGameModule PakalPlayer)`.
   - The data path in the code: `Assets/mygame/hello.txt`.

4. **Configure and build** in the Developer shell:

   ```
   cmake --preset win64
   cmake --build build-win64
   ```

   The first configure takes several minutes, because it builds bgfx inside `extern/Pakal` and the vcpkg packages.
   Afterwards `git status` shows `extern/Pakal` as modified (the engine patches bgfx). That is expected; don't commit
   it and don't revert it.

5. **Run it and check the log:**

   ```
   cd build-win64\bin
   PakalPlayer.exe MyGameModule.dll
   ```

   The console must show:

   ```
   [GameModuleLoader] Loaded module 'MyGameModule.dll'
   [MyGameModule] started; Assets/mygame/hello.txt Hello from ...
   ```

   If it says `is missing`, the data path or the `DATA` folder name is wrong. Close the window to stop it. If you start
   it from a script, stop it by its process id, never by its name: other games may be running.

6. **Commit:** `git add -A && git commit -m "A new game on Pakal"`. Don't add `build-win64/`.

## Adding the game itself

The module's `start()` pushes the first **game state**. A state is a screen: a menu, a level. Template:

```cpp
// source/PlayState.h
#pragma once
#include "BaseGameState.h"

namespace MyGame
{
	class PlayState : public Pakal::BaseGameState
	{
	protected:
		void on_initialize(Pakal::Engine* engine) override;    // load what the state needs
		void on_terminate(Pakal::Engine* engine) override;     // free it
		void on_update(unsigned long dtMilliseconds) override; // every frame
		void on_activate(Pakal::Engine* engine) override {}
		void on_deactivate(Pakal::Engine* engine) override {}
	};
}
```

In the module's `start()`:

```cpp
engine.game_state_manager()->push_state(new MyGame::PlayState(), true);
```

Add each new `.cpp` to `SOURCES` in `CMakeLists.txt`. Include `"GameStateManager.h"` and `"PlayState.h"` in the module.

If the game's own code uses a package directly (for example nlohmann_json for its data files), find it in the game's
`CMakeLists.txt` and link it there. The engine's `find_package` is not visible from the game's folder:

```cmake
find_package(nlohmann_json CONFIG REQUIRED)
# and in pakal_add_game_module: LINKS Pakal::TopDownView nlohmann_json::nlohmann_json
```

**Which libraries to link** (`LINKS` in `pakal_add_game_module`):

| The game | Link | Then read |
|---|---|---|
| top-down action (shooter, brawler, RPG; 3D models, levels as data) | `Pakal::TopDownView` | `extern/Pakal/docs/TOPDOWN_MODULE.md` |
| a level editor in the game | `Pakal::TopDownEditor` | `extern/Pakal/docs/EDITOR.md` |
| network play | `Pakal::TopDownNet` | `extern/Pakal/docs/NETWORK.md` |
| Lua scripts | `Pakal::Scripting` | `extern/Pakal/docs/TOPDOWN_MODULE.md` |
| quests and missions | `Pakal::Quests` (+ `Pakal::QuestsView` for the debug panel) | `extern/Pakal/docs/QUEST_GUIDE.md` |
| anything else | `Pakal::Pakal` alone (always linked) | `extern/Pakal/docs/ARCHITECTURE.md` |

For a top-down game, copy the pattern of the engine's examples, but don't link or include from them:
- `extern/Pakal/examples/brawler_proto` is small: a module, one level state, and its data in `data/`;
- `extern/Pakal/examples/dungeon_proto` adds a menu and network play.

Free 3D models: `extern/Pakal/docs/ASSET_INDEX.md` lists the CC0 packs, and `tools/fetch_free_assets.py` unpacks them.

## Rules

- **Never edit `extern/Pakal`.** If the game needs something from the engine, write down what and why, and tell the
  person. The change goes to the engine's repository, and the game then moves its submodule to the commit that has
  it (`git -C extern/Pakal checkout <commit>`, then commit the new pointer).
- **Include only the engine's interfaces.** Never include a backend's header (`bgfx/...`, `SFML/...`, `box2d/...`)
  and never name a backend class. Engine ADR-0017 says so, and the engine's build checks it.
- **Don't include or link anything from `extern/Pakal/examples`.** Copy what you need into `source/` or `data/` and
  make it the game's own.
- **Keep the game's own interface files** (RmlUi `.rml`/`.rcss`, fonts) in `data/`. Ship each font with its license
  file.
- **Data goes in `data/`.** It is copied to `bin/Assets/<game>/`; read it by that path.

## When something fails

| Symptom | Cause, fix |
|---|---|
| `Could not find a package configuration file provided by "nlohmann_json"` (or `RmlUi`, `assimp`...) | Configured without the preset, or `VCPKG_ROOT` is not set. Use `cmake --preset win64` in the Developer shell. |
| `bgfx, bx or bimg is missing from external/` | The engine's submodules were not fetched: `git submodule update --init --recursive`. |
| `pakal_add_game_module` unknown | `add_subdirectory(extern/Pakal pakal)` must come before it. |
| Paths under your own folder where the engine's should be (`<my-game>/source/...` not found), or the engine's tests and examples in your build | The engine's checkout predates its submodule support. Move `extern/Pakal` to a newer commit. |
| The player starts but logs `module not found` | Run `PakalPlayer.exe` from `build-win64\bin`, with the DLL's file name. |
| Link errors naming bgfx or Box2D symbols | The game includes a backend. Use the engine's interfaces instead (see Rules). |

## Not covered yet

- **Android builds** of a game in its own repository: the engine's Android build makes the APK its own way.
- **Linux.** The engine builds there, but this guide's preset is Windows only.

More detail: `extern/Pakal/docs/GAME_REPOSITORY.md`.
