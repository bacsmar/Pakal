# Quick start

From an empty folder to the engine running. The details and other options are in [BUILDING.md](BUILDING.md).

## Windows

You need Visual Studio 2022 or 2026, with the "Desktop development with C++" workload, and
[vcpkg](https://github.com/microsoft/vcpkg) cloned and bootstrapped. From an "x64 Native Tools Command Prompt":

```bat
git clone https://github.com/bacsmar/Pakal.git C:\dev\Pakal
cd C:\dev\Pakal
git submodule update --init --recursive
set VCPKG_ROOT=C:\path\to\vcpkg
cmake --preset win64
cmake --build --preset win64-release
build-win64\bin\PakalFeatureLab.exe
```

- **bgfx:** the first `cmake --preset win64` also builds bgfx and its shader compiler from the submodules. It takes a
  few minutes, once.
- **Short folder:** clone into a short folder. Some of bgfx's third-party sources have deep paths, and Windows stops at
  260 characters.
- **No assets needed:** `PakalFeatureLab` shows 2D sprites, a camera and generated sounds, so you can see the engine runs.
- **Tests:** `cd build-win64 && ctest -j 8 -E Mcp`.

## Linux (Ubuntu 24.04)

Install the packages listed in [Dockerfile.linux](../Dockerfile.linux) (it also builds RmlUi), or build inside that image.
Then:

```sh
git clone https://github.com/bacsmar/Pakal.git && cd Pakal
git submodule update --init --recursive
cmake --preset linux-release
cmake --build --preset linux-release
cd build-linux && ctest -j 4 -E Mcp
```

## Android

`python android/build_apk.py --game <name>` builds an APK. The NDK, SDK and JDK paths are in [ANDROID.md](ANDROID.md).

## Next

- **The 3D examples** use free CC0 model packs that are not in the repository. `python tools/fetch_free_assets.py`
  puts them in place, then `python tools/zombies_convert/build_downtown.py --meshes-only` (with Blender) exports the
  models. Configure again and build: Brawler and Dungeon then run. For the Zombies example, also
  `python -m pip install -r tools/requirements.txt` and `python tools/zombies_convert/build_downtown.py`, then
  `set ZOMBIES_LEVEL=Assets\zombies\downtown_survival_01\level.json` and run `ZombiesModule.dll`: it plays with free
  rules, silent ([ZOMBIES_DOWNTOWN.md](ZOMBIES_DOWNTOWN.md)).
- **Running a game module:** `build-win64\bin\PakalPlayer.exe ZombiesModule.dll`, from the repository root.
- **Opening a level in the editor:** add `--edit <level.json>` ([EDITOR.md](EDITOR.md)).
- **How the engine is put together:** [ARCHITECTURE.md](ARCHITECTURE.md).
