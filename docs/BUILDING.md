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

## Next Steps

After successfully building:
- Read [USAGE.md](USAGE.md) for programming guide
- Check [BGFX_BACKEND.md](BGFX_BACKEND.md) for bgfx-specific features
- Explore examples in `examples/` directory
- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand engine design
