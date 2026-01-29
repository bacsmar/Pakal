# bgfx Graphics Backend

## Overview

The bgfx graphics backend provides a modern, cross-platform rendering solution for Pakal Engine. bgfx is a high-performance rendering library that abstracts multiple rendering APIs (Vulkan, DirectX 11/12, Metal, OpenGL, OpenGL ES) behind a unified interface.

## Features

- **Cross-platform**: Supports Windows, Linux, macOS, Android, iOS, and more
- **Multiple rendering APIs**: Automatically selects the best renderer for your platform
  - Windows: DirectX 11/12, Vulkan, OpenGL
  - Linux: Vulkan, OpenGL
  - macOS: Metal
  - Mobile: OpenGL ES, Vulkan, Metal
- **Modern architecture**: Efficient command-buffer based rendering
- **Debug visualization**: Support for Box2D physics debug drawing

## Building with bgfx

### Prerequisites

- C++11 compatible compiler
- CMake 3.10+ (if using CMake build system)
- Git (for submodules)

### Enabling bgfx Backend

1. **Initialize git submodules** (if not already done):
   ```bash
   git submodule update --init --recursive
   ```

2. **Enable bgfx in Config.h**:
   Edit `source/Config.h` and set:
   ```cpp
   #define PAKAL_USE_BGFX 1
   #define PAKAL_USE_IRRLICHT 0  // Cannot use both simultaneously
   ```

3. **Build the project**:
   - The build system will automatically include bgfx source files
   - bgfx will compile alongside your project
   - First build may take longer due to bgfx compilation

### Platform-Specific Notes

#### Windows
- bgfx will auto-select DirectX 11 by default, or DirectX 12 if available
- You can force a specific renderer via environment variables or runtime configuration
- Visual Studio 2017 or later recommended

#### Linux
- Requires development libraries: `libx11-dev`, `libgl1-mesa-dev`
- Install via: `sudo apt-get install libx11-dev libgl1-mesa-dev`
- bgfx will prefer Vulkan if available, otherwise OpenGL

#### macOS
- Requires macOS 10.14+ for Metal support
- Xcode 10.0 or later recommended
- Metal is the default and recommended renderer on macOS

## Architecture

### BgfxGraphicsSystem

The main graphics system class that manages bgfx initialization and rendering:

```cpp
class BgfxGraphicsSystem : public GraphicsSystem
{
    // Initializes bgfx with window handle from SFML
    void on_init_graphics(const WindowArgs& args) override;
    
    // Main render loop - calls bgfx::frame()
    void on_update_graphics(long long dt) override;
    
    // Cleanup bgfx resources
    void on_terminate_graphics() override;
};
```

### Window Integration

bgfx integrates with the existing SFML window manager:
- SFML creates and manages the window
- SFML provides the native window handle
- bgfx renders to that window using the native handle
- Input is still handled by SFML

### Debug Drawing

The `BgfxDebugDrawer` class implements Box2D debug visualization:
- Draws physics bodies, joints, and contacts
- Uses bgfx debug draw API for efficient line rendering
- Automatically integrated when PAKAL_USE_BOX2D is enabled

## Current Limitations

This is the initial implementation with the following limitations:

1. **No sprite rendering yet**: SpriteComponent_Bgfx is not implemented
2. **No advanced materials**: Simple debug rendering only
3. **No texture loading**: TextureResource_Bgfx not yet available
4. **No 3D support**: Currently focused on 2D rendering
5. **No shader customization**: Uses bgfx built-in shaders
6. **No post-processing**: No effects pipeline yet
7. **Basic debug drawing**: Limited to simple primitives

## Future Enhancements

Planned features for future releases:

- [ ] Sprite rendering system with batching
- [ ] Texture loading via bimg
- [ ] Camera system (orthographic and perspective)
- [ ] Material system
- [ ] Shader loading and hot-reloading
- [ ] Post-processing effects
- [ ] Render-to-texture support
- [ ] Text rendering
- [ ] UI integration (Dear ImGui support)
- [ ] Particle systems
- [ ] Advanced debug visualization

## Troubleshooting

### bgfx fails to initialize

**Problem**: bgfx::init() returns false

**Solutions**:
- Check that the window handle is valid
- Ensure graphics drivers are up to date
- Try forcing a different renderer via `BGFX_RENDERER_TYPE` environment variable
- Check console output for bgfx error messages

### Black screen / Nothing renders

**Problem**: Window opens but stays black

**Solutions**:
- Verify bgfx initialized successfully (check logs)
- Ensure window dimensions are valid (> 0)
- Check that `bgfx::frame()` is being called each frame
- Verify vsync settings if frame rate is 0

### Build errors with bgfx

**Problem**: Compilation errors in bgfx source files

**Solutions**:
- Ensure submodules are fully initialized: `git submodule update --init --recursive`
- Check that you have a C++11 compatible compiler
- Update your compiler to the latest version
- Check bgfx compatibility with your platform

### Performance issues

**Problem**: Low frame rate or stuttering

**Solutions**:
- Enable vsync: `graphic_system_settings.vsync = true`
- Check debug mode isn't enabled in bgfx
- Verify you're not calling expensive operations in the render loop
- Profile using bgfx's built-in profiler

## API Reference

### Getting Started with bgfx Backend

```cpp
#include "Engine.h"
#include "IPakalApplication.h"

using namespace Pakal;

class MyApp : public IPakalApplication
{
public:
    void start(Engine& engine) override
    {
        // Engine is already initialized with bgfx
        // when PAKAL_USE_BGFX=1
        
        // Your game initialization here
    }
    
    void end(Engine& engine) override
    {
        // Cleanup
    }
    
    const char* get_name() override { return "MyBgfxApp"; }
};

int main()
{
    Engine::Settings settings;
    settings.graphic_system_settings.resolution = {1280, 720};
    settings.graphic_system_settings.vsync = true;
    
    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

### Debug Drawing Example

```cpp
#if PAKAL_USE_BOX2D == 1
// The debug drawer is automatically created and registered
// when you add debug drawing to your physics system

auto* physics = engine.get_physics_system();
physics->set_debug_draw_flags(
    b2Draw::e_shapeBit | 
    b2Draw::e_jointBit
);
#endif
```

## Resources

- [bgfx GitHub Repository](https://github.com/bkaradzic/bgfx)
- [bgfx Documentation](https://bkaradzic.github.io/bgfx/)
- [bgfx Examples](https://github.com/bkaradzic/bgfx/tree/master/examples)
- [Pakal Engine Documentation](../docs/README.md)

## Contributing

When contributing to the bgfx backend:

1. Follow the existing code style
2. Test on multiple platforms when possible
3. Update documentation for any new features
4. Ensure backward compatibility with Irrlicht backend
5. Add appropriate error handling and logging

## License

The bgfx backend integration follows the Pakal Engine license. bgfx itself is licensed under the BSD 2-clause license.
