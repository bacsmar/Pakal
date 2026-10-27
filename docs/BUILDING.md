# Building Pakal Engine

This guide covers building Pakal Engine from source on various platforms.

## Prerequisites

### All Platforms
- C++11 compatible compiler
- Git (for cloning and managing submodules)
- CMake 3.10+ (recommended) or your preferred build system

### Platform-Specific

#### Windows
- Visual Studio 2017 or later (2019/2022 recommended)
- Windows SDK

#### Linux
- GCC 7.0+ or Clang 5.0+
- Development libraries:
  ```bash
  sudo apt-get install build-essential libx11-dev libgl1-mesa-dev
  ```

#### macOS
- Xcode 10.0 or later
- Xcode Command Line Tools
- macOS 10.14+ (for Metal support with bgfx)

## Getting the Source

### Clone the Repository

```bash
git clone https://github.com/bacsmar/Pakal.git
cd Pakal
```

### Initialize Submodules

Pakal uses git submodules for external dependencies. You must initialize them:

```bash
git submodule update --init --recursive
```

This will download:
- bgfx and its dependencies (bx, bimg) in `external/`
- Any other third-party libraries

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

### 2. Build bgfx (once, or after changing the submodule)

bgfx has no CMake project in this repo. Its own tools generate and build it. Run from a Developer shell:

```bat
cd external\bgfx
..\bx\tools\bin\windows\genie.exe --with-tools --with-dynamic-runtime vs2022
cd .build\projects\vs2022
msbuild bx.vcxproj     /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild bimg.vcxproj   /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild bimg_decode.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild bgfx.vcxproj   /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
msbuild shaderc.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
```

`PlatformToolset=v145` is the toolset of Visual Studio 2026. Use `v143` with Visual Studio 2022. The libraries are written to `external\bgfx\.build\win64_vs2022\bin`.

### 3. Configure and build

Set `VCPKG_ROOT` to your vcpkg folder, then from a Developer shell:

```bat
set VCPKG_ROOT=C:\Users\<you>\vcpkg
cmake --preset win64
cmake --build --preset win64-release
```

The build output is in `build-win64\bin`: `PakalPlayer.exe`, `Pakal.dll`, `ContraGameModule.dll`, and the bgfx shaders for Direct3D 11.

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
- Tools: `get_status` (engine and graphics frame rates) and `screenshot` (`{"path": "...tga"}`; the file is written on the next frame, so wait a moment before reading it).
- Quick check: `curl -X POST http://127.0.0.1:8765/mcp -H "Content-Type: application/json" -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}"`.

### 7. Audio

Spatial audio uses the SFML backend (`SoundManagerSFML`). The listener is set with `set_listener(position, forward, up)`. Sounds with a position are heard relative to it, with `min_distance` and `attenuation` from `SoundSettings`. SFML only spatializes mono sounds.

- `AudioTests` checks the distance gain and left/right pan math. It needs no audio device.
- `PakalAudioDemo [toneHz] [distance]` plays a tone at left, right, front and behind the listener. Its output prints the expected pan and gain. **Listen to it**: the sound should move left, right, front and behind in that order. This cannot be checked automatically.

Known gaps on Windows:
- Some ContraGame textures live in `examples/ContraGame/Assets/generated`, which is generated and not in the repo. Without them the game uses placeholder textures.
- Only the Release configuration of bgfx is built. Debug needs the same steps with `Debug` in place of `Release`.

## Configuration

### Graphics Backend Selection

Edit `source/Config.h` to choose your graphics backend:

#### Using bgfx (Modern, Cross-platform)
```cpp
#define PAKAL_USE_BGFX 1
#define PAKAL_USE_IRRLICHT 0
```

#### Using Irrlicht (Legacy)
```cpp
#define PAKAL_USE_BGFX 0
#define PAKAL_USE_IRRLICHT 1
```

**Note**: You cannot enable both simultaneously. The build will fail with an error if both are enabled.

### Other Configuration Options

In `source/Config.h`:
```cpp
#define PAKAL_USE_SCRIPTS 1        // Lua scripting support
#define PAKAL_USE_BOX2D 1          // Box2D physics
#define PAKAL_USE_SFML_AUDIO 1     // SFML audio system
#define PAKAL_USE_SFML_INPUT 1     // SFML input handling
#define PAKAL_USE_SFML_WINDOW 1    // SFML window management
#define PAKAL_USE_ROCKET 1         // Rocket GUI (requires Irrlicht)
```

## Building

### Windows (Visual Studio)

1. Open the Visual Studio solution file (`.sln`) in the repository root or `vs2012/` directory
2. Select your configuration (Debug/Release) and platform (x86/x64)
3. Build the solution (F7 or Build → Build Solution)
4. Output will be in `bin/` or `Build/` directory

**Note**: First build with bgfx will take longer (5-10 minutes) as bgfx compiles its shaders and rendering backends.

### Linux (Make/CMake)

#### Using CMake (Recommended)

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

#### Using Make (if available)

```bash
make -j$(nproc)
```

### macOS (Xcode)

#### Using CMake + Xcode

```bash
mkdir build
cd build
cmake -G Xcode ..
open Pakal.xcodeproj
```

Then build in Xcode (⌘B).

#### Using Make

```bash
make -j$(sysctl -n hw.ncpu)
```

## Build Targets

Common build targets:
- `Pakal` - Main engine library
- `PakalApp` - Example application
- `tests` - Unit tests (if available)

## Runtime Dependencies

### Required Libraries

Depending on your configuration, you'll need these libraries at runtime:

#### SFML
- `sfml-audio`
- `sfml-system`
- `sfml-window`

#### Irrlicht (if using Irrlicht backend)
- `Irrlicht.dll/.so/.dylib`

#### Box2D (if enabled)
- Usually statically linked

#### Lua (if scripting enabled)
- `lua` (version 5.x)

### Library Paths

Make sure libraries are in your system path:
- **Windows**: Copy DLLs to executable directory or add to PATH
- **Linux**: Add to LD_LIBRARY_PATH or install system-wide
- **macOS**: Add to DYLD_LIBRARY_PATH or use `@rpath`

## Testing the Build

### Run Example Application

```bash
# Windows
bin\Release\PakalApp.exe

# Linux/macOS
./bin/PakalApp
```

### Verify Graphics Backend

Check the console output at startup:
```
[BgfxGraphicsSystem] Initializing bgfx graphics system
[BgfxGraphicsSystem] Renderer: Direct3D 11
```
or for Irrlicht:
```
[IrrGraphicsSystem] Initializing Irrlicht graphics system
```

## Troubleshooting

### Submodules Not Initialized

**Problem**: Build fails with missing includes for bgfx

**Solution**:
```bash
git submodule update --init --recursive
```

### bgfx Build Errors

**Problem**: Compilation errors in bgfx source files

**Solution**:
- Ensure you have a C++11 compatible compiler
- Update your compiler to the latest version
- Check bgfx platform compatibility

### Missing SFML

**Problem**: Linker errors about SFML libraries

**Solution**:
- Install SFML development libraries
- On Linux: `sudo apt-get install libsfml-dev`
- On macOS: `brew install sfml`
- On Windows: Ensure SFML libraries are in your project or system path

### Missing Irrlicht

**Problem**: Can't find Irrlicht.dll

**Solution**:
- If using Irrlicht backend, ensure Irrlicht is installed
- Copy Irrlicht DLL to executable directory
- Or switch to bgfx backend: `#define PAKAL_USE_BGFX 1`

### Performance Issues

**Problem**: Very low frame rate or stuttering

**Solution**:
- Make sure you're building in Release mode (not Debug)
- Enable compiler optimizations
- Check graphics driver is up to date
- Try different graphics backend

## Advanced Build Options

### Static vs Shared Libraries

Edit your build configuration to choose:
- **Static**: Easier distribution, larger executable
- **Shared**: Smaller executable, requires DLLs

### Custom bgfx Configuration

bgfx has many configuration options. Create a custom `bgfx_config.h` if needed.

### Cross-Compilation

For mobile or embedded targets, refer to platform-specific documentation.

## Continuous Integration

Pakal uses CI for automated testing. Check `.github/workflows/` for CI configuration examples.

## Getting Help

If you encounter build issues:

1. Check the [GitHub Issues](https://github.com/bacsmar/Pakal/issues)
2. Review [bgfx documentation](https://bkaradzic.github.io/bgfx/) for backend-specific issues
3. Create a new issue with:
   - Your platform and compiler version
   - Full build error log
   - Configuration settings (Config.h)

## Further Reading

After successfully building:
- Read [USAGE.md](USAGE.md) for programming guide
- Check [BGFX_BACKEND.md](BGFX_BACKEND.md) for bgfx-specific features
- Explore examples in `examples/` directory
- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand engine design
