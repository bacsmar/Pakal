# Ejemplos prácticos

1) Entidad con Sprite + Física

```c++
// se asume engine creado y m_entity_manager/m_component_manager inicializados
EntityHandle e = engine.entity_manager()->create();

// crear componente sprite (Irrlicht)
auto spriteComp = engine.component_manager()->createComponent("SpriteComponent_Irrlicht", e);
// inicializar con sprite sheet / texture (pseudocódigo)
spriteComp->initialize({ .texture = "assets/player.png", .layer = 0 });

// crear componente físico (Box2D)
auto physComp = engine.component_manager()->createComponent("SpritePhysicsComponent", e);
physComp->initialize({ .sprite_physics = spritePhysicsSettings, .position = {0,0,0} });

// ahora la sincronización visual/física la maneja la implementación de los componentes
```

2) Cámara

```c++
// obtener graphics system y crear cámara
auto gs = engine.graphics_system();
// muchas implementaciones exponen helpers para crear nodos de cámara; aquí pseudocódigo:
CameraComponentSettings cs;
cs.position = {0,0,10};
engine.component_manager()->createComponent("CameraComponent_Irrlicht", entity)->initialize(cs);
```

3) Cargar recurso

```c++
ResourceManager::instance().add_source(std::make_shared<DirectorySource>("assets/"));
auto mesh = ResourceManager::instance().load<Mesh>("models/ship.obj");
```

4) Rocket GUI (crear documento simple)

```c++
// en la inicialización del GraphicsSystem
rocket::Core::Context* ctx = /* obtener contexto */;
auto doc = ctx->CreateDocumentFromMemory("<div>Hello Pakal</div>");
doc->Show();
```

Notas: los ejemplos combinan pseudocódigo y llamadas reales. Ajustes menores pueden ser necesarios según la versión concreta de cada componente en el repo.

5) Crear entidad especializada

```c++
// Definir entidad personalizada
class PlayerEntity : public Entity
{
    float m_health;
    Vector3 m_spawnPoint;
    
public:
    using Entity::Entity;  // Heredar constructor
    
    BasicTaskPtr initialize() override {
        m_health = 100.0f;
        m_spawnPoint = Vector3(0, 0, 0);
        
        // Inicializar componentes si es necesario
        return TaskUtils::create_completed_task();
    }
    
    BasicTaskPtr terminate() override {
        return TaskUtils::create_completed_task();
    }
    
    void take_damage(float amount) {
        m_health -= amount;
        if (m_health <= 0) {
            respawn();
        }
    }
    
    void respawn() {
        m_health = 100.0f;
        // Lógica de respawn
    }
};

// Registrar factory (en tu provider)
entityManager->add_entity_factory(
    new TFactory<Entity, PlayerEntity>("PlayerEntity")
);

// Crear instancia
PlayerEntity* player = entityManager->create_entity<PlayerEntity>();
player->take_damage(25.0f);
```

Ver `docs/ENTITY_PATTERNS.md` para más patrones y guías sobre cuándo usar entidades especializadas vs genéricas.
