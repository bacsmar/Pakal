# Uso rápido de Pakal (ejemplo mínimo)

Requisitos previos
- Compilador C++ moderno compatible con C++11/C++14.
- Dependencias opcionales según backends: Irrlicht, Box2D, SFML.

Compilación
1. Clona el repositorio: `git clone https://github.com/bacsmar/Pakal.git`
2. Configura las opciones de compilación en `CMakeLists.txt` o el sistema de build que uses (asegúrate de activar `PAKAL_USE_IRRLICHT=1` y `PAKAL_USE_BOX2D=1` si quieres render + física).

Ejemplo mínimo en C++ — `main.cpp`

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
    const char* get_name() const override { return "MyApp"; }
};

int main()
{
    Engine::Settings settings;
    // Allocators: elegir implementaciones concretas
    settings.graphic_system_allocator = [](Engine* e, const GraphicsSystem::Settings& s){ return new IrrGraphicsSystem(static_cast<const IrrGraphicsSystem::Settings&>(s)); };
    settings.physics_system_allocator = [](Engine* e, const PhysicsSystem::Settings& s){ return new Box2DPhysicsSystem(static_cast<const Box2DPhysicsSystem::Settings&>(s)); };
    settings.sound_manager_allocator = [](Engine* e){ return new SoundManager(); };

    Engine engine(settings);
    MyApp app;
    engine.run(&app);
    return 0;
}
```

Consejos
- Para desarrollar con Rocket GUI asegúrate de compilar el módulo Rocket (`PAKAL_USE_ROCKET=1`) y revisar `source/rocketgui/`.
- Para debugging visual de física, activa `PhysicsSystem::Settings::debug_draw` y el `GraphicsSystem` dibujará las formas.
