# Building Pakal Engine

How to build Pakal from source on Windows and Linux. The short version is [QUICKSTART.md](QUICKSTART.md); Android is in
[ANDROID.md](ANDROID.md).

## What you need

- A C++20 compiler: MSVC from Visual Studio 2022 or 2026 on Windows, GCC 13 or later on Linux.
- CMake 3.21 or later and Ninja (both come with Visual Studio's C++ workload).
- Git, and Python 3 for the tools.
- The submodules, which hold bgfx, bx and bimg in `external/`:

  ```bash
  git clone https://github.com/bacsmar/Pakal.git
  cd Pakal
  git submodule update --init --recursive
  ```

## Windows (x64, CMake + vcpkg)

Current Windows build: MSVC (Visual Studio 2022 or 2026), CMake + Ninja, vcpkg for SFML and Box2D, and bgfx built from the submodule.

Pakal.dll is a shared library. ContraGameModule.dll and PakalPlayer.exe link to it, so the engine singletons exist once. Dependencies are static, with the dynamic CRT (`x64-windows-static-md`).

### 1. Dependencies

- Visual Studio with the "Desktop development with C++" workload (MSVC, CMake, Ninja).
- vcpkg, cloned somewhere outside the repo, bootstrapped once:
  ```bat
  git clone https://github.com/microsoft/vcpkg.git C:\Users\<you>\vcpkg
  C:\Users\<you>\vcpkg\bootstrap-vcpkg.bat -disableMetrics
  ```
- Submodules: `git submodule update --init --recursive`.

### 2. bgfx (built for you)

`cmake --preset win64` builds bgfx, bx, bimg, shaderc and texturec from the submodules the first time, with bgfx's own tools
(GENie, then MSBuild with your compiler's toolset), after applying our patches (`cmake/BgfxBuild.cmake`). It takes a few
minutes, once; it builds them again when the submodules or our patches change. Clone into a short folder: shaderc's
third-party sources have deep paths, and past 260 characters MSBuild reports "Cannot open source file".

To build them yourself instead, configure with `-DPAKAL_BGFX_AUTOBUILD=OFF` and run, from a Developer shell:

```bat
cd external\bgfx
..\bx\tools\bin\windows\genie.exe --with-tools --with-dynamic-runtime vs2022
cd .build\projects\vs2022
msbuild bx.vcxproj     /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild bimg.vcxproj   /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild bimg_decode.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild bgfx.vcxproj   /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild shaderc.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild texturec.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
```

`PlatformToolset=v145` is the toolset of Visual Studio 2026. Use `v143` with Visual Studio 2022. The libraries are written to `external\bgfx\.build\win64_vs2022\bin`.

Pakal carries a few fixes to bgfx as patches (`patches/bgfx/*.patch`, e.g. the OpenGL barrier that lets draws read
what a compute shader wrote), not a fork. Every build applies them to the submodule when it configures, the ones it does
not have yet (`cmake/BgfxPatches.cmake`), and the desktop build then builds bgfx again with them. Building bgfx by hand
(`PAKAL_BGFX_AUTOBUILD=OFF`), configure Pakal once first (it warns when it has just patched bgfx: rebuild its libraries
then), or apply them yourself: `git -C external/bgfx apply --ignore-whitespace ../../patches/bgfx/<name>.patch`.
The submodule is marked `ignore = dirty` in `.gitmodules`, so the patched files do not show as changes.

### 3. Configure and build

Set `VCPKG_ROOT` to your vcpkg folder, then from a Developer shell:

```bat
set VCPKG_ROOT=C:\Users\<you>\vcpkg
cmake --preset win64
cmake --build --preset win64-release
```

The build output is in `build-win64\bin`: `PakalPlayer.exe`, `Pakal.dll`, `ContraGameModule.dll`, and the bgfx shaders for Direct3D 11.

### 3b. The core alone

Every build also builds `PakalCore` (`cmake/PakalCore.cmake`): the engine's core without its backends (no bgfx, Box2D,
SFML, Assimp or RmlUi; the dummy graphics, physics, UI, window and input systems take their place), and `CoreTests`,
which starts the engine with it, runs its systems and ends it. The target only sees `source/`, so a core file that
includes a backend breaks it. To build only the core (bgfx need not be built):

```bat
cmake --preset win64-core
cmake --build --preset win64-core-release
build-core\bin\CoreTests.exe
```

### 4. Run ContraGame

```bat
cd build-win64\bin
PakalPlayer.exe ContraGameModule.dll
```

### 5. Run the mesh viewer

`PakalMeshViewer` shows one model with a directional light and a sky dome. It reads any model Assimp supports. Its arguments are `<model> [seconds] [scale] [pitchDegrees] [screenshot.tga]`:

```bat
PakalMeshViewer.exe path\to\model.dae 10 1 0 shot.tga
```

Skinned models (with bones) are animated on the GPU. The viewer plays the first clip, switches to the second at a third of the session and returns to the first at two thirds. Clips come from the `<animation_clip>` entries of the Collada file. Screenshots are taken during clip 0 (`shot.tga`) and clip 1 (`shot_clip1.tga`).

Limits of the current loader:
- Assimp rejects some exporter output. Collada files from the Zombies data need cleaning first: `<extra>` blocks inside `<vertices>`, `<instance_effect>` references to other `.bdae` files, and animation channels that target non-transform properties such as `AuraIntensity`. This cleaning belongs in the data converter.
- Effect meshes (for example `FX_*`) are drawn like any other mesh. The converter should drop them.

Assimp is installed by vcpkg (`assimp` in `vcpkg.json`). On Linux it is required too: `libassimp-dev`.

### 6. Run the physics test

`PhysicsTests` checks the top-down 2D world: a character that walks in four directions without overlapping walls, stops against a wall, and ray casts that hit walls and enemies. It reads two text files:

- `collision.txt`: lines `rect cx cy width height name` and `circle cx cy diameter name`, in level units (100 units per meter).
- `spawn.txt`: lines `spawn x y name`. The first one is the start point.

```bat
PhysicsTests.exe collision.txt spawn.txt
```

The engine owns the Box2D world (`PhysicsWorld2D`). Test and game code must not link Box2D themselves: two copies in one process break the contact registry.

### 6b. Run the gameplay test

`GameplayTests` runs the gameplay core on the same level data: navigation (A*), enemy chase and attack, hitscan shots, the kill objective and pickups. It needs one more file than the physics test:

- `entities.txt`: lines `enemy x y speed name` and `pickup x y name`, in level units.

```bat
GameplayTests.exe collision.txt spawn.txt entities.txt
```

### 6e. Level scripts and the objectives (Lua)

The level's objectives run the game's own Lua scripts. The converter copies each objective's `OnActivate`, `Run` and `OnDeactivate` text into `level.json` (`script.objectives`) and the scene order into `script.order`. The module runs them with Lua 5.5 (vcpkg `lua`): `Wait(ms)` suspends a script on the game clock, and the game's functions are bound in `examples/zombies_module/ZombieScript.cpp`. Functions the level does not implement yet are logged once and do nothing (the cutscene calls, for example).

Debug variables, for checking the objective without aiming:

- `ZOMBIES_DEBUG_KILL_EVERY=<seconds>` removes one living zombie at that interval.
- `ZOMBIES_AUTOFIRE=1` holds fire.
- The periodic log line shows the objective variables: `A`, `killCount`, `enemyCount`.

### 6d. Control the running game from an AI client (MCP)

PakalPlayer can serve a small Model Context Protocol server (JSON-RPC 2.0 over HTTP, `POST /mcp`) on the loopback address only. It is off unless you set the port:

```bat
set PAKAL_MCP_PORT=8765
set PAKAL_MCP_TOKEN=change-me
PakalPlayer.exe ZombiesModule.dll
```

- `PAKAL_MCP_TOKEN` (optional): every request must send `Authorization: Bearer <token>`. Without a token the port is still loopback-only.
- Tools: the engine's (source/mcp/EngineTools): `get_status`, `screenshot` (`{"path": "...tga"}`; the file is written on the next frame, so wait a moment before reading it), and the profiling and tuning ones (`get_stats`, `get_profile`, `get_settings`, `set_settings`, `set_camera`, `record_start`/`record_stop`): docs/PROFILING.md, which `tools/pakal_device.py` drives.
- Quick check: `curl -X POST http://127.0.0.1:8765/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}"`.

### 7. Audio

Spatial audio uses the SFML backend (`SoundManagerSFML`). The listener is set with `set_listener(position, forward, up)`. Sounds with a position are heard relative to it, with `min_distance` and `attenuation` from `SoundSettings`. SFML only spatializes mono sounds.

- `AudioTests` checks the distance gain and left/right pan math. It needs no audio device.
- `PakalAudioDemo [toneHz] [distance]` plays a tone at left, right, front and behind the listener. Its output prints the expected pan and gain. **Listen to it**: the sound should move left, right, front and behind in that order. This cannot be checked automatically.

Known gaps on Windows:
- Some ContraGame textures live in `examples/ContraGame/Assets/generated`, which is generated and not in the repo. Without them the game uses placeholder textures.
- Only the Release configuration of bgfx is built. Debug needs the same steps with `Debug` in place of `Release`.

## Linux x64 (GCC, OpenGL or Vulkan)

### Native

Install the packages and RmlUi 6.3 that `Dockerfile.linux` installs (the commands are also in [AGENTS.md](../AGENTS.md)),
then:

```bash
cmake --preset linux-release
cmake --build --preset linux-release
ctest --test-dir build-linux -E Mcp --output-on-failure
```

bgfx and its shader compiler are built from the submodules on the first configure, as on Windows. The engine shaders
are compiled for OpenGL 4.3 and SPIR-V. Outputs are in `build-linux/bin` and `build-linux/lib`; launch the player from
`build-linux/bin` with a Linux game module (`.so`). Windows DLLs cannot be loaded by the Linux player.

### In Docker

`tools/build_linux.sh` does the same inside the `Dockerfile.linux` image, so nothing is installed on the host. Set
`PAKAL_BUILD_JOBS` to control parallelism (default: 4). CI runs the same image.

On a Linux desktop with X11 or XWayland, `tools/run_linux.sh [module.so]` runs a module with the image's libraries and
software OpenGL (ContraGame by default). It forwards the X authority and the PulseAudio socket when they exist, and the
`PAKAL_*` and `ZOMBIES_*` variables you set.

### Notes

- bgfx owns the graphics context; the SFML backend only supplies the window and its events.
- The desktop X11 visual is opaque, so the back buffer is RGB.
- If NVIDIA EGL under XWayland stalls on vblank, set `PAKAL_VSYNC=0`; Pakal's own frame cap stays active.
- Integrated Intel GPUs: [LINUX_INTEL_PERFORMANCE.md](LINUX_INTEL_PERFORMANCE.md).

## Troubleshooting

- **Missing bgfx headers or `genie`:** the submodules are not there. Run `git submodule update --init --recursive`.
- **"Cannot open source file" while bgfx builds on Windows:** the path is past 260 characters. Clone into a short folder.
- **bgfx fails to build:** the full log is `build-<preset>/bgfx_build.log`.
- **Very low frame rate:** check that you built Release, not Debug.

## Further reading

- [ARCHITECTURE.md](ARCHITECTURE.md): how the engine is put together.
- [USAGE.md](USAGE.md): writing an application against the engine.
- [BGFX_BACKEND.md](BGFX_BACKEND.md): the renderer.
- `.github/workflows/build.yml`: what CI builds and tests on every pull request.
