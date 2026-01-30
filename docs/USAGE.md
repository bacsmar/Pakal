# Uso rápido de Pakal (ejemplo mínimo)

## Requisitos previos
- Compilador C++ moderno compatible con C++11/C++14.
- Dependencias opcionales según backends: 
  - bgfx (moderno, cross-platform) - **Recomendado**
  - Irrlicht (legacy)
  - Box2D (física 2D)
  - SFML (audio, input, ventanas)

## Compilación
1. Clona el repositorio: `git clone https://github.com/bacsmar/Pakal.git`
2. Inicializa submodules: `git submodule update --init --recursive`
3. Configura el backend gráfico en `source/Config.h`:
   - Para bgfx: `#define PAKAL_USE_BGFX 1` y `#define PAKAL_USE_IRRLICHT 0`
   - Para Irrlicht: `#define PAKAL_USE_BGFX 0` y `#define PAKAL_USE_IRRLICHT 1`
4. Compila el proyecto (ver [BUILDING.md](BUILDING.md) para detalles)

## Ejemplo mínimo con bgfx — `main.cpp`

```c++
#include "Engine.h"
#include "IPakalApplication.h"

using namespace Pakal;

class MyApp : public IPakalApplication
{
public:
    void start(Engine& engine) override
    {
        // Con PAKAL_USE_BGFX=1, el sistema gráfico bgfx ya está inicializado
        // Aquí puedes registrar recursos, crear entidades, etc.
        
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
    
    // Configuración del sistema gráfico
    settings.graphic_system_settings.resolution = {1280, 720};
    settings.graphic_system_settings.vsync = true;
    settings.graphic_system_settings.full_screen = false;
    
    // Los allocators se configuran automáticamente según Config.h
    // Si PAKAL_USE_BGFX=1, usará BgfxGraphicsSystem
    // Si PAKAL_USE_IRRLICHT=1, usará IrrGraphicsSystem
    
    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

## Ejemplo mínimo con Irrlicht (legacy) — `main.cpp`

```c++
#include "Engine.h"
#include "IPakalApplication.h"

using namespace Pakal;

class MyApp : public IPakalApplication
{
public:
    void start(Engine& engine) override
    {
        // registrar recursos, crear entidades, etc.
    }
    void end(Engine& engine) override {}
    const char* get_name() override { return "MyApp"; }
};

int main()
{
    Engine::Settings settings;
    
    // Con PAKAL_USE_IRRLICHT=1, los allocators ya están configurados
    // No necesitas especificarlos manualmente
    
    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

## Configuración avanzada (allocators manuales)

Si necesitas personalizar los allocators:

```c++
Engine::Settings settings;

// Allocator del sistema gráfico (ejemplo manual)
#if PAKAL_USE_BGFX == 1
settings.graphic_system_allocator = [](Engine* e, const GraphicsSystem::Settings& s) { 
    return new BgfxGraphicsSystem(s); 
};
#elif PAKAL_USE_IRRLICHT == 1
settings.graphic_system_allocator = [](Engine* e, const GraphicsSystem::Settings& s) { 
    return new IrrGraphicsSystem(s); 
};
#endif

// Allocator del sistema de física
#if PAKAL_USE_BOX2D == 1
settings.physics_system_allocator = [](Engine* e, const PhysicsSystem::Settings& s) { 
    return new Box2DPhysicsSystem(s); 
};
#endif

// Allocator del gestor de sonido
#if PAKAL_USE_SFML_AUDIO == 1
settings.sound_manager_allocator = [](Engine* e) { 
    return new SoundManagerSFML(); 
};
#endif
```

## Trabajando con entidades y componentes

```c++
void start(Engine& engine) override
{
    // Crear una entidad
    auto* entity = engine.entity_manager()->create_entity();
    
    // Añadir un componente de transformación
    entity->set_position(tmath::vector3df(100, 100, 0));
    
    // TODO: Cuando SpriteComponent_Bgfx esté implementado:
    // auto* sprite = entity->add_component<SpriteComponent>();
    // sprite->initialize({...});
}
```

## Debug drawing de física (Box2D)

```c++
void start(Engine& engine) override
{
    #if PAKAL_USE_BOX2D == 1
    auto* physics = engine.get_physics_system();
    
    // Activar debug drawing
    physics->set_debug_draw_enabled(true);
    
    // Configurar flags de debug
    // (esto funcionará automáticamente con bgfx o Irrlicht)
    #endif
}
```

## Consejos

### Backend gráfico bgfx
- **Ventajas**: Moderno, cross-platform, múltiples APIs (Vulkan, DX12, Metal, etc.)
- **Estado actual**: Debug drawing implementado, sprites pendientes
- Ver [BGFX_BACKEND.md](BGFX_BACKEND.md) para más detalles

### Backend gráfico Irrlicht
- **Ventajas**: Completo, estable, con soporte para Rocket GUI
- **Desventajas**: Backend más antiguo, mantenimiento limitado
- Para Rocket GUI asegúrate de compilar con `PAKAL_USE_ROCKET=1`

### Física con Box2D
- Activa `PAKAL_USE_BOX2D=1` en Config.h
- El debug drawing funciona con ambos backends (bgfx e Irrlicht)

### Debugging
- Activa logs: `#define PAKAL_USE_LOG 1` en Config.h
- Para información de archivo/línea: `#define PAKAL_LOG_SHOW_FILE_INFO 1`

### Rendimiento
- Compila en modo Release para mejor rendimiento
- Activa vsync para frame rate estable
- En bgfx, el renderer se selecciona automáticamente (puedes forzarlo con variables de entorno)

## Próximos pasos

- Lee [ARCHITECTURE.md](ARCHITECTURE.md) para entender la arquitectura del motor
- Revisa [BGFX_BACKEND.md](BGFX_BACKEND.md) para características específicas de bgfx
- Explora [EXAMPLES.md](EXAMPLES.md) para ejemplos más complejos
- Consulta [SUBSYSTEMS.md](SUBSYSTEMS.md) para detalles de cada subsistema

