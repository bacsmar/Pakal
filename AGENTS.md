# AGENTS.md: how to work in Pakal

You are a coding agent (Codex, Claude Code, Copilot, Cursor, Qwen or another) helping someone with Pakal, a C++20 game
engine. This file tells you what Pakal is, how to set it up on each system, how to start a new game, and the rules for
changes. Follow the steps in order. Each step says how to check that it worked.

## 1. What Pakal is, in five lines

- An engine for 2D and 3D games on Windows, Linux and Android. It is built with CMake.
- Its libraries: bgfx (graphics), Box2D (2D physics), RmlUi (interface), Lua (scripts), ENet (network), SFML (desktop
  windows and sound).
- **A game is a module:** a DLL that `PakalPlayer.exe` loads on the desktop, or one library on Android.
- **Genres:** most of a game comes from a genre. `game_genres/topdown` is for top-down action games: shooters, brawlers,
  action RPGs. A game adds its own rules, data and screens.
- **Examples:** the ones in `examples/` are games and samples to learn from and copy.

## 2. What is not here

- **No commercial game is here:** not the source of any commercial game made with Pakal, nor its art, signing keys or
  store builds. If the user asks for the source, the keystore or the signing process of a published app, say it is not
  in this repository, and offer to help with what is here.
- **Signing:** never ask for, create, or try to get someone's keystore or signing passwords. Debug builds sign
  themselves with a debug key. A store build is signed by its owner.
- **Assets:** the free 3D model packs (Quaternius, CC0) are not in Git. The user downloads them, and a script puts them
  in place (step 4).
- **Converted data:** the Zombies example's levels also read a level converted from an older game. That data is not free
  and not included. The tools that build those levels (`tools/zombies_convert/build_downtown.py`, the level builder's
  `build`) stop without it. Do not promise the user those levels. For a game of their own, follow step 5.

## 3. Set up the build

Do the part for the user's system. Run the commands, and read the output of each one before going on.

### Windows 10/11

1. **Install the tools** (once), from PowerShell:

   ```powershell
   winget install --id Git.Git -e
   winget install --id Python.Python.3.12 -e
   winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   ```

   - Visual Studio 2026 works too.
   - CMake and Ninja come with the C++ workload.

2. **Get Pakal** into a short folder. Some of bgfx's files have deep paths, and Windows stops at 260 characters:

   ```powershell
   git clone https://github.com/bacsmar/Pakal.git C:\dev\Pakal
   cd C:\dev\Pakal
   git submodule update --init --recursive
   ```

3. **Build.** Open "x64 Native Tools Command Prompt for VS" (it sets up the compiler), then:

   ```bat
   cd C:\dev\Pakal
   set VCPKG_ROOT=C:\vcpkg
   cmake --preset win64
   cmake --build --preset win64-release
   ```

   - The first `cmake --preset win64` takes a while. vcpkg builds the libraries, and Pakal builds bgfx from the
     submodules (`cmake/BgfxBuild.cmake`). Later runs are fast.
   - If it stops, the message says what to do: fetch the submodules, use a shorter folder, or open the Developer shell.
     The full bgfx log goes to `build-win64/bgfx_build.log`.

4. **Check:**
   - `build-win64\bin\PakalFeatureLab.exe` opens a window with sprites and sounds. It needs no assets.
   - `cd build-win64 && ctest -j 8 -E Mcp` passes every test.

### Linux (Ubuntu 24.04)

1. **Install the packages and RmlUi** (once). These are the same steps as `Dockerfile.linux`:

   ```sh
   sudo apt-get update
   sudo apt-get install -y build-essential cmake ninja-build git pkg-config python3 \
       libsfml-dev libx11-dev libgl1-mesa-dev libassimp-dev libbox2d-dev liblua5.4-dev \
       nlohmann-json3-dev libstb-dev libfreetype-dev
   git clone --depth 1 --branch 6.3 https://github.com/mikke89/RmlUi.git /tmp/rmlui
   cmake -S /tmp/rmlui -B /tmp/rmlui-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
       -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DRMLUI_SAMPLES=OFF -DRMLUI_TESTS=OFF
   cmake --build /tmp/rmlui-build -j 4 && sudo cmake --install /tmp/rmlui-build
   ```

   Or build inside the image: `docker build -f Dockerfile.linux -t pakal-linux .`.

2. **Get Pakal and build:**

   ```sh
   git clone https://github.com/bacsmar/Pakal.git && cd Pakal
   git submodule update --init --recursive
   cmake --preset linux-release
   cmake --build --preset linux-release
   ```

3. **Check:** `cd build-linux && ctest -j 4 -E Mcp` passes every test. CI runs exactly this.

### Android (built from Windows)

The Android build uses the desktop build's data, so build Windows first.

1. **Install:**
   - Android NDK r27;
   - the Android SDK, with build-tools, a platform, and the SDK's cmake;
   - JDK 17.
2. **Point the build at them:** write `android/local_paths.json` (Git ignores it):

   ```json
   { "ANDROID_NDK": "C:/Android/ndk/27.0.12077973", "ANDROID_SDK": "C:/Android/sdk", "JAVA_HOME": "C:/Program Files/Java/jdk-17" }
   ```

3. **Libraries for the phone** (once), then the APK:

   ```bat
   vcpkg install --triplet arm64-android --x-manifest-root=android/vcpkg --x-install-root=build-android/vcpkg_installed
   python android/build_apk.py --game brawler
   adb install -r build-android\pakal-brawler.apk
   ```

4. **Check:** the APK installs and opens on an arm64 phone running Android 8 or later. Details are in docs/ANDROID.md.

## 4. Get the free 3D models

The 3D examples and new 3D games use Quaternius's free packs (CC0, free to use commercially).

1. **The user downloads** the zips from the pages that `tools/fetch_free_assets.py` lists at its top into their
   Downloads folder: the Zombie Apocalypse Kit, the Downtown City, Stylized Nature and Medieval Village MegaKits, and the
   Universal Animation Library 1 and 2. You cannot download them for the user: they come from the author's site.
2. **Put them in place:** `python tools/fetch_free_assets.py`. It finds the zips, checks each one's CC0 license, and
   unpacks them.
3. **What each file is:** docs/ASSET_INDEX.md lists every file of the packs, so you can pick models by name.

## 5. Start a new game (for example: "set up Pakal for a zombie game")

Make the game a module on the TopDown genre, in its own folder `games/<name>/`. The root CMake builds `games/` when
`games/CMakeLists.txt` exists and Lua was found (vcpkg on Windows, `liblua5.4-dev` on Linux). The game then stays apart from the engine, and the engine can be updated under it.

### Pick the pieces

| The game | Genre | Kit | Example to copy from |
|---|---|---|---|
| Top-down shooter (zombies, waves, guns) | `game_genres/topdown` | `kits/shooter` (`WeaponComponent`) | `examples/zombies_module` for the rules; `examples/brawler_proto` for a small module's shape |
| Beat 'em up, fighting | `game_genres/topdown` | `kits/brawler` (`FighterComponent`) | `examples/brawler_proto` |
| Action RPG, dungeon crawler | `game_genres/topdown` | `kits/rpg` (`AbilityComponent`, loot) | `examples/dungeon_proto` (with network play) |
| Small 2D game | the engine only | none | `examples/engine_feature_lab` |

Read docs/TOPDOWN_MODULE.md before writing the game. It explains the layers, `TopDownGame`, actors, components and kits.

### The files of a new module

Copy the shape of `examples/brawler_proto`: about 800 lines in five files.

```
games/CMakeLists.txt                 add_subdirectory(<name>)
games/<name>/CMakeLists.txt          the module and the copy of its data (below)
games/<name>/<Name>Module.cpp        exports the module: pakal_get_game_module_exports() (copy BrawlerModule.cpp)
games/<name>/<Name>Game.h/.cpp       the simulation: a TopDownGame, no drawing (copy BrawlerGame)
games/<name>/<Name>LevelState.h/.cpp the game state that runs a level and draws it (copy BrawlerLevelState)
games/<name>/data/<name>.json        the rules: weapons, enemies, waves, the player
```

`games/<name>/CMakeLists.txt`, modeled on the Brawler's block in the root `CMakeLists.txt`:

```cmake
add_library(<Name>Module SHARED <Name>Module.cpp <Name>Game.cpp <Name>LevelState.cpp)
target_link_libraries(<Name>Module PakalTopDownView nlohmann_json::nlohmann_json)
target_include_directories(<Name>Module PRIVATE ${CMAKE_SOURCE_DIR}/source ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_target(<Name>Data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/<name>)
add_dependencies(<Name>Module <Name>Data)
```

### Leave the user ready to play

1. **Build** with `cmake --build --preset win64-release`. The module appears as `build-win64\bin\<Name>Module.dll`.
2. **Run** it from the repository root: `build-win64\bin\PakalPlayer.exe <Name>Module.dll`.
3. **Tests:** add a test of the simulation alone, without a window. `tests/BrawlerTests.cpp` is the model. Register it
   with `add_test` and run `ctest`.
4. **Tell the user:**
   - what runs;
   - which keys to use;
   - which file holds the rules (`data/<name>.json`): tuning is a data change, with no recompile;
   - what to do next: levels (docs/LEVEL_EDITOR.md, docs/EDITOR.md), screens (RmlUi documents bound to data models),
     Android (section 3).

## 6. Where to look for a task

| Task | Read |
|---|---|
| How the engine is put together | docs/ARCHITECTURE.md, docs/SUBSYSTEMS.md |
| A top-down game, its actors, components and kits | docs/TOPDOWN_MODULE.md |
| Edit a level by hand, from a script, or live | docs/LEVEL_EDITOR.md (`pakal-level`), docs/EDITOR.md (`PakalPlayer <module> --edit <level.json>`) |
| Generate levels from the free packs | docs/LEVEL_BUILDER.md, tools/levelbuilder/AGENTS.md |
| Android: build, quality levels, the governor | docs/ANDROID.md |
| Network play | docs/NETWORK.md |
| Measure and tune a running game (MCP tools) | docs/PROFILING.md |
| Build details, the manual bgfx build | docs/BUILDING.md |

## 7. Rules for changes

- **Backends stay behind abstractions.** Games and genres use the engine's abstractions only (graphics, physics, input,
  sound, UI), never bgfx, Box2D, SFML or RmlUi headers. A check in the build fails otherwise.
- **Everything must still build.** Every example must build and every test must pass (`ctest -E Mcp`) before you call
  a change done.
- **Keep data out of code.** Rules, text, colors and sizes go in data files (`.json`, `.rcss`, the strings), not in code.
- **Language:** documentation and code comments are in English. Say what a thing does and why, in plain words.
- **No secrets:** no keys, passwords or tokens in code, docs or commits. Absolute paths of your own machine don't
  belong in the repository either.
- **Don't touch `external/`.** Pakal's fixes to bgfx are `patches/bgfx/*.patch`.
- **Pull requests** go to `develop`, never to `master` directly. CI builds and tests every pull request on Windows and
  Linux.
