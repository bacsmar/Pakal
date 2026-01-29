# Subsistemas de Pakal

## GraphicsSystem
Implementación principal: `IrrGraphicsSystem` (archivos: `source/irrlicht/IrrGraphicsSystem.h`, `source/irrlicht/IrrGraphicsSystem.cpp`).

Responsabilidades:
- Inicializar `irr::IrrlichtDevice` y `IVideoDriver`.
- Manejar `begin_scene`, `draw`, `end_scene`.
- Proveer batchers: `SceneNodeBatcher`, `SpriteBatcher`.
- Manejar `MaterialManager` y debug drawers.
- Integrar Rocket GUI mediante `IrrRocketRenderer` / `IrrRocketGUI`.

Notas y ejemplos:
- En `on_init_graphics` se crea el dispositivo con `SIrrlichtCreationParameters` y se registran fuentes de recursos:
  - DirectorySourceIrrlitch, ZipSourceIrrlitch.
- En `on_update_graphics` el flujo es: `begin_scene()` -> `draw()` -> `m_ui_manager->draw_ui()` -> `end_scene()`.

## PhysicsSystem
Implementación principal: `Box2DPhysicsSystem` (`source/box2D/Box2DPhysicsSystem.h`).

Responsabilidades:
- Crear y mantener `b2World`.
- Crear/destruir cuerpos y joints.
- Proveer debug drawer para visualizar shapes en `GraphicsSystem`.

Notas:
- Configurable mediante `PhysicsSystem::Settings` (gravity, iterations, continuous physics, etc.).

## Entity / Component System
Archivos relevantes: `source/ComponentManager.*`, `source/EntityManager.*`, `source/components/*`.

Modelo:
- `EntityManager` gestiona entidades (IDs) y sus componentes.
- `ComponentManager` registra `IComponentProvider`s (cada System implementa provider) y factories que crean componentes concretos (Ej: `MeshComponent_Irrlicht`).
- Componentes tienen ciclo de vida mediante `initialize()` y `terminate()` que devuelven `BasicTaskPtr` (sistema de tareas asíncronas).

## ResourceManager &amp; Persistencia
- `ResourceManager` permite registrar fuentes (Directory, Zip), localizar y cargar recursos.
- La persistencia y serialización se manejan vía `Archive` y un proyecto relacionado `pakal_persist` (fork en `bacsmar/pakal_persist`).

## OSManager / Input / Sound / UI
- `OSManager` maneja ventanas y eventos. Puede usar SFML o wrappers nativos (Android) dependiendo de defines.
- `IInputManager` se obtiene con `os_manager()->get_input_manager()` y se registra como provider de componentes.
- `SoundManager` backend configurable (SFML u otra implementación).
- UI: Rocket GUI integrado con `IrrGraphicsSystem` mediante `IrrRocketRenderer`.

## Otros
- Task system: `BasicTask`, `TaskUtils` para inicialización/terminación en paralelo.
- GameStateManager: maneja estados de juego y su actualización.

Referencias a archivos y clases en el repo para profundizar.
