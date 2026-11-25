# A game in its own repository

A game can live in its own repository with the engine as a git submodule. The game's CMake project adds the engine with
`add_subdirectory`. It gets:

- `PakalPlayer`;
- the engine's libraries by name (`Pakal::Pakal`, `Pakal::TopDownView`...);
- `pakal_add_game_module()`.

The engine's examples, tests and tools stay out of its build.

`examples/game_repository` is a working starting point. `GameRepositoryTests` configures it against the engine's tree
on every ctest run.

## Set up

```
git init my-game && cd my-game
git submodule add <the engine's url> extern/Pakal
git -C extern/Pakal checkout <the engine's commit or tag>
git submodule update --init --recursive        # the engine's own submodules: bgfx, bx, bimg, enet
cp -r extern/Pakal/examples/game_repository/* .
```

The game's `CMakeLists.txt`, from the example:

```cmake
cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
project(MyGame CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # the module next to PakalPlayer

add_subdirectory(extern/Pakal pakal)

pakal_add_game_module(MyGameModule
    SOURCES source/MyGameModule.cpp
    LINKS Pakal::TopDownView
    DATA data mygame)                                           # data/ -> bin/Assets/mygame
add_dependencies(MyGameModule PakalPlayer)
```

## Configure and build (Windows)

The engine's packages come from vcpkg with the engine's manifest. A preset in the game's `CMakePresets.json` points to
it:

```json
{
  "name": "win64",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build-win64",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
    "VCPKG_TARGET_TRIPLET": "x64-windows-static-md",
    "VCPKG_MANIFEST_DIR": "${sourceDir}/extern/Pakal"
  }
}
```

From a Developer shell (vcvars64) with `VCPKG_ROOT` set:

```
cmake --preset win64
cmake --build build-win64
build-win64\bin\PakalPlayer.exe MyGameModule
```

The first configure builds bgfx inside `extern/Pakal/external` (a few minutes, once). The engine applies its bgfx patches
there too, so the submodule shows as modified; that is expected.

## What the engine gives a game's project

- **Libraries.** `Pakal::Pakal` (the engine), plus:
  - `Pakal::TopDown`, `Pakal::TopDownView`, `Pakal::TopDownEditor`, `Pakal::TopDownNet`;
  - `Pakal::Net`, `Pakal::Scripting` (Lua), `Pakal::LevelDoc`;
  - `Pakal::Quests`, `Pakal::QuestsView`;
  - `Pakal::Platformer` and `Pakal::Versus`, when the engine's tree has them.

  Each brings its headers, C++20 and the engine's compile definitions. A game names no folder of the engine.
- **Packages the game uses directly** are found by the game itself, because the engine's `find_package` calls are not
  visible from the game's folder. For example, the game calls `find_package(nlohmann_json CONFIG REQUIRED)` and then
  links `nlohmann_json::nlohmann_json`. The engine's vcpkg manifest already lists what the engine uses; anything else
  needs a manifest of the game's own.
- **`pakal_add_game_module()`** (`cmake/PakalGameModule.cmake`). It builds the module as a library next to the player and
  copies its data under `Assets/`.
- **`PakalPlayer`**, and `PakalPlayerWindowed` on Windows (no console, for a game handed out).
- **Shaders.** They are compiled into `bin/shaders/<backend>`.

## Options

| Option | Default | What |
|---|---|---|
| `PAKAL_BUILD_EXAMPLES` | ON only when the engine is the top project | Zombies, the prototypes, the viewers |
| `PAKAL_BUILD_TESTS` | the same | the engine's tests, added to ctest |
| `PAKAL_BUILD_TOOLS` | the same | `pakal-level` |

A game sets one ON before `add_subdirectory` to get it, for example `set(PAKAL_BUILD_TOOLS ON)` for `pakal-level`.
`games/` and `cmake/LocalGames.cmake` are read only when the engine is the top project.

## Rules for a game

- Use the engine's abstractions only, never a backend's headers (bgfx, SFML, Box2D). See `docs/adr` ADR-0017.
- Don't patch the submodule. A change the engine needs goes to the engine's repository, and the game moves its
  submodule to the commit that has it.
- Bring the game's own interface files (style sheets, fonts with their licenses). The engine's examples are not part of
  a game's build.

## Not covered yet

- **Android.** `android/CMakeLists.txt` builds the engine and one game into a `.so` its own way.
- **A packaged SDK.** `find_package(Pakal)` and an installed engine are ADR-0016's later step. The library names above
  are the ones that package will export, so a game's CMake will not change when it arrives.
