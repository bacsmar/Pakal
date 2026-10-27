# Pakal quick start (minimal example)

## Prerequisites
- Modern C++ compiler compatible with C++11/C++14.
- Optional dependencies depending on the backends: 
  - bgfx (modern, cross-platform) - **Recommended**
  - Irrlicht (legacy)
  - Box2D (2D physics)
  - SFML (audio, input, windows)

## Building
1. Clone the repository: `git clone https://github.com/bacsmar/Pakal.git`
2. Initialize submodules: `git submodule update --init --recursive`
3. Configure the graphics backend in `source/Config.h`:
   - For bgfx: `#define PAKAL_USE_BGFX 1` and `#define PAKAL_USE_IRRLICHT 0`
   - For Irrlicht: `#define PAKAL_USE_BGFX 0` and `#define PAKAL_USE_IRRLICHT 1`
4. Build the project (see [BUILDING.md](BUILDING.md) for details)

## Minimal example with bgfx — `main.cpp`

```c++
#include "Engine.h"
#include "IPakalApplication.h"

using namespace Pakal;

class MyApp : public IPakalApplication
{
public:
    void start(Engine& engine) override
    {
        // With PAKAL_USE_BGFX=1 the bgfx graphics system is already initialized
        // Register resources, create entities, etc. here
        
        LOG_INFO("MyApp started with bgfx backend!");
    }
    
    void end(Engine& engine) override
    {
        LOG_INFO("MyApp ending");
    }
    
    const char* get_name() override { return "MyApp"; }
};

int main()
{
    Engine::Settings settings;
    
    // Graphics system settings
    settings.graphic_system_settings.resolution = {1280, 720};
    settings.graphic_system_settings.vsync = true;
    settings.graphic_system_settings.full_screen = false;
    
    // The allocators are set automatically from Config.h
    // Si PAKAL_USE_BGFX=1, usará BgfxGraphicsSystem
    // Si PAKAL_USE_IRRLICHT=1, usará IrrGraphicsSystem
    
    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

## Minimal example with Irrlicht (legacy) — `main.cpp`

```c++
#include "Engine.h"
#include "IPakalApplication.h"

using namespace Pakal;

class MyApp : public IPakalApplication
{
public:
    void start(Engine& engine) override
    {
        // register resources, create entities, etc.
    }
    void end(Engine& engine) override {}
    const char* get_name() override { return "MyApp"; }
};

int main()
{
    Engine::Settings settings;
    
    // With PAKAL_USE_IRRLICHT=1 the allocators are already set
    // No necesitas especificarlos manualmente
    
    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

## Advanced configuration (manual allocators)

If you need to customize the allocators:

```c++
Engine::Settings settings;

// Graphics system allocator (manual example)
#if PAKAL_USE_BGFX == 1
settings.graphic_system_allocator = [](Engine* e, const GraphicsSystem::Settings& s) { 
    return new BgfxGraphicsSystem(s); 
};
#elif PAKAL_USE_IRRLICHT == 1
settings.graphic_system_allocator = [](Engine* e, const GraphicsSystem::Settings& s) { 
    return new IrrGraphicsSystem(s); 
};
#endif

// Physics system allocator
#if PAKAL_USE_BOX2D == 1
settings.physics_system_allocator = [](Engine* e, const PhysicsSystem::Settings& s) { 
    return new Box2DPhysicsSystem(s); 
};
#endif

// Sound manager allocator
#if PAKAL_USE_SFML_AUDIO == 1
settings.sound_manager_allocator = [](Engine* e) { 
    return new SoundManagerSFML(); 
};
#endif
```

## Working with entities and components

```c++
void start(Engine& engine) override
{
    // Create an entity
    auto* entity = engine.entity_manager()->create_entity();
    
    // Place it
    entity->set_position(tmath::vector3df(100, 100, 0));
    
    // Components are added the same way:
    // auto* sprite = entity->add_component<SpriteComponent>();
    // sprite->initialize({...});
}
```

## Physics debug drawing (Box2D)

```c++
void start(Engine& engine) override
{
    #if PAKAL_USE_BOX2D == 1
    auto* physics = engine.get_physics_system();
    
    // Turn debug drawing on
    physics->set_debug_draw_enabled(true);
    
    // Debug flags
    // (this works the same with bgfx or Irrlicht)
    #endif
}
```

## Tips

### bgfx graphics backend
- **Advantages**: Modern, cross-platform, multiple APIs (Vulkan, DX12, Metal, etc.)
- See [BGFX_BACKEND.md](BGFX_BACKEND.md) for more details

### Irrlicht graphics backend
- **Advantages**: Complete, stable, with Rocket GUI support
- **Disadvantages**: Older backend, limited maintenance
- For Rocket GUI make sure to build with `PAKAL_USE_ROCKET=1`

### Physics with Box2D
- Enable `PAKAL_USE_BOX2D=1` in Config.h
- Debug drawing works with both backends (bgfx and Irrlicht)

### Debugging
- Enable logs: `#define PAKAL_USE_LOG 1` in Config.h
- For file/line information: `#define PAKAL_LOG_SHOW_FILE_INFO 1`

### Performance
- Build in Release mode for better performance
- Enable vsync for a stable frame rate
- In bgfx, the renderer is selected automatically (you can force it with environment variables)

## Further reading

- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand the engine architecture
- Check [BGFX_BACKEND.md](BGFX_BACKEND.md) for bgfx-specific features
- Explore [EXAMPLES.md](EXAMPLES.md) for more complex examples
- See [SUBSYSTEMS.md](SUBSYSTEMS.md) for details on each subsystem

