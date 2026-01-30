# Pakal Engine

Pakal is a modular game engine written in C++ with support for multiple graphics and physics backends.

## Features

- **Multiple Graphics Backends**:
  - **bgfx** - Modern, cross-platform (Vulkan, DirectX 11/12, Metal, OpenGL) - **Recommended**
  - Irrlicht - Legacy support
- **Physics**: Box2D integration for 2D physics
- **Audio**: SFML audio system
- **Input**: SFML input handling
- **Windowing**: SFML window management
- **Scripting**: Lua support (optional)
- **GUI**: Rocket GUI integration (with Irrlicht backend)

## Quick Start

### Prerequisites

```bash
# Clone the repository
git clone https://github.com/bacsmar/Pakal.git
cd Pakal

# Initialize submodules (required for bgfx)
git submodule update --init --recursive
```

### Configuration

Edit `source/Config.h` to enable bgfx:

```cpp
#define PAKAL_USE_BGFX 1       // Enable bgfx backend
#define PAKAL_USE_IRRLICHT 0   // Disable Irrlicht
```

### Build

See [docs/BUILDING.md](docs/BUILDING.md) for detailed build instructions.

### Simple Example

```cpp
#include "Engine.h"
#include "IPakalApplication.h"

using namespace Pakal;

class MyApp : public IPakalApplication
{
public:
    void start(Engine& engine) override
    {
        // Your game initialization
    }
    
    void end(Engine& engine) override {}
    const char* get_name() override { return "MyApp"; }
};

int main()
{
    Engine::Settings settings;
    settings.graphic_system_settings.resolution = {1280, 720};
    
    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

## Documentation

La documentación del proyecto se encuentra en `docs/README.md`. Incluye arquitectura del motor, descripción de subsistemas y ejemplos de uso.

- [BUILDING.md](docs/BUILDING.md) - Build instructions
- [USAGE.md](docs/USAGE.md) - Usage examples and API guide
- [BGFX_BACKEND.md](docs/BGFX_BACKEND.md) - bgfx backend documentation
- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - Engine architecture
- [SUBSYSTEMS.md](docs/SUBSYSTEMS.md) - Subsystem details

## Supported Platforms

- **Windows** - DirectX 11/12, Vulkan, OpenGL
- **Linux** - Vulkan, OpenGL
- **macOS** - Metal
- Android - OpenGL ES, Vulkan (with bgfx backend)
- iOS - Metal (with bgfx backend)

## License

See LICENSE file for details.

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting PRs.
