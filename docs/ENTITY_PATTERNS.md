# Patrones de Entidades en Pakal

## Introducción

Este documento explica la arquitectura de entidades en Pakal, respondiendo preguntas comunes sobre cuándo usar diferentes tipos de entidades, patrones de diseño, y comparaciones con motores como Unity y Unreal Engine.

## Arquitectura Actual de Entidades

Pakal implementa un sistema Entity-Component donde:

- **`Entity`** (clase base abstracta): Define la interfaz mínima para todas las entidades
  - Métodos virtuales puros: `initialize()`, `terminate()`
  - Provee acceso a componentes mediante `get_component<T>()`
  - Mantiene referencia a su descriptor (`EntityDescriptor`)
  
- **`GenericEntity`** (implementación concreta): Entidad flexible que almacena componentes dinámicamente
  - Contenedor de componentes (`std::vector<Component*>`)
  - Sistema de eventos (`evt_initialize`, `evt_terminate`, `evt_update`)
  - Apropiada para la mayoría de casos de uso

- **`EntityDescriptor`**: Define plantillas/arquetipos para crear entidades preconfiguradas
  - Permite serialización y persistencia
  - Facilita la creación de entidades desde datos externos

## ¿Cuándo Usar Múltiples Tipos de Entidades?

### Caso 1: Entidades Especializadas (Recomendado para lógica compleja)

Crear subclases de `Entity` cuando:

1. **Performance crítica**: La entidad requiere optimización específica
   ```cpp
   // Ejemplo: Entidad de proyectil optimizada
   class ProjectileEntity : public Entity
   {
       Vector3 m_velocity;
       float m_lifetime;
       
   public:
       BasicTaskPtr initialize() override {
           // Lógica específica sin overhead de búsqueda de componentes
           return TaskUtils::completed_task();
       }
       
       void update(float dt) {
           m_lifetime -= dt;
           // Física simple sin componentes pesados
       }
   };
   ```

2. **Lógica de negocio específica**: Comportamiento único que no encaja en componentes
   ```cpp
   // Ejemplo: Entidad de jugador con lógica de input/estado
   class PlayerEntity : public Entity
   {
       PlayerState m_state;
       InputBuffer m_input;
       
   public:
       void handle_input(const InputEvent& event) {
           // Lógica específica de jugador
       }
   };
   ```

3. **Interfaz especializada**: API específica para un tipo de objeto
   ```cpp
   // Ejemplo: Vehículo con API específica
   class VehicleEntity : public Entity
   {
   public:
       void accelerate(float force);
       void steer(float angle);
       void brake(float force);
       float get_speed() const;
   };
   ```

### Caso 2: GenericEntity (Recomendado para flexibilidad)

Usar `GenericEntity` cuando:

1. **Prototipado rápido**: Necesitas iterar rápidamente
2. **Composición dinámica**: Los componentes cambian en runtime
3. **Entidades definidas por datos**: Cargadas desde archivos/descriptores
4. **Casos generales**: La mayoría de entidades de juego

## Entidades como Template vs Concretas

### Patrón Template (EntityDescriptor)

**Ventajas:**
- Reutilización: Define arquetipos una vez, instancia múltiples veces
- Separación datos/código: Diseñadores pueden crear entidades sin programar
- Serialización: Fácil de guardar/cargar desde archivos
- Consistencia: Todas las instancias del mismo tipo son idénticas

**Ejemplo de uso:**
```cpp
// 1. Crear descriptor (una vez, desde código o archivo)
EntityDescriptor* enemyDesc = new EntityDescriptor("BasicEnemy");
// Configurar componentes, propiedades, etc.
entityManager->add_descriptor(enemyDesc);

// 2. Instanciar múltiples entidades desde el template
Entity* enemy1 = entityManager->create_entity("GenericEntity", "BasicEnemy");
Entity* enemy2 = entityManager->create_entity("GenericEntity", "BasicEnemy");
Entity* enemy3 = entityManager->create_entity("GenericEntity", "BasicEnemy");
```

**Cuándo usar:**
- Enemigos, items, obstáculos con configuración estándar
- Niveles con muchas instancias del mismo tipo
- Sistemas de prefabs/arquetipos

### Patrón Concreto (Clases especializadas)

**Ventajas:**
- Type-safety: Errores detectados en compilación
- Performance: Sin indirección ni búsqueda de componentes
- API clara: Métodos específicos del tipo
- Encapsulación: Lógica interna protegida

**Ejemplo:**
```cpp
class BossEntity : public Entity
{
    enum class Phase { INTRO, PHASE1, PHASE2, PHASE3, DEFEATED };
    Phase m_currentPhase;
    float m_health;
    
public:
    BasicTaskPtr initialize() override;
    void update(float dt);
    void take_damage(float amount);
    void change_phase(Phase newPhase);
};

// Uso directo sin casting ni búsqueda
BossEntity* boss = entityManager->create_entity<BossEntity>();
boss->take_damage(50.0f);
```

**Cuándo usar:**
- Entidades únicas (jefe final, jugador principal)
- Lógica compleja con múltiples estados
- Performance crítica (partículas, proyectiles en masa)

## Comparación con Unity y Unreal

### Unity

Unity usa **MonoBehaviour + GameObject**:
- **GameObject**: Similar a `GenericEntity` - contenedor genérico
- **MonoBehaviour**: Componentes con scripts personalizados
- Todo es genérico por defecto, especialización mediante componentes

**Equivalencia en Pakal:**
```cpp
// Unity: GameObject con componentes
GameObject player = new GameObject("Player");
player.AddComponent<SpriteRenderer>();
player.AddComponent<PlayerController>();

// Pakal: GenericEntity con componentes
GenericEntity* player = entityManager->create_entity<GenericEntity>();
player->add_component(componentManager->create_component<SpriteComponent>());
player->add_component(componentManager->create_component<PlayerControllerComponent>());
```

### Unreal Engine

Unreal usa **Actor + UObject**:
- **Actor**: Clase base para entidades en el mundo
- Especialización mediante **herencia de C++** (ACharacter, APawn, APlayerController)
- Sistema de componentes complementa pero no reemplaza la jerarquía

**Equivalencia en Pakal:**
```cpp
// Unreal: Especialización por herencia
class AMyCharacter : public ACharacter
{
    void BeginPlay() override;
    void Tick(float DeltaTime) override;
};

// Pakal: Mismo patrón
class MyCharacterEntity : public Entity
{
    BasicTaskPtr initialize() override;
    void update(float deltaTime);
};
```

## Recomendaciones de Diseño

### Estrategia Híbrida (Recomendada)

Combina ambos enfoques según necesidad:

1. **Base genérica**: Usa `GenericEntity` + descriptores para el 80% de entidades
2. **Especializaciones selectivas**: Crea subclases para casos críticos
3. **Componentes reutilizables**: Lógica compartida en componentes

```cpp
// Entidades comunes - GenericEntity + descriptores
Entity* tree = entityManager->create_entity("GenericEntity", "TreeDescriptor");
Entity* rock = entityManager->create_entity("GenericEntity", "RockDescriptor");

// Entidades especiales - Subclases concretas
PlayerEntity* player = entityManager->create_entity<PlayerEntity>();
BossEntity* boss = entityManager->create_entity<BossEntity>();
```

### Criterios de Decisión

| Criterio | GenericEntity | Subclase Especializada |
|----------|---------------|------------------------|
| Cantidad de instancias | Muchas | Pocas/única |
| Complejidad de lógica | Baja/Media | Alta |
| Necesidad de performance | Normal | Crítica |
| Cambios en runtime | Frecuentes | Raros |
| Definición por datos | Sí | No |
| Type-safety requerido | No | Sí |

## Patrones Avanzados

### 1. Factory Pattern con IEntityProvider

Registra factories para crear tipos personalizados:

```cpp
class MyEntityProvider : public IEntityProvider
{
public:
    void register_entities(std::vector<IFactory<Entity>*>& factories) override
    {
        factories.push_back(new TFactory<Entity, PlayerEntity>("PlayerEntity"));
        factories.push_back(new TFactory<Entity, BossEntity>("BossEntity"));
    }
    
    void register_descriptors(std::vector<IFactory<EntityDescriptor>*>& factories) override
    {
        // Registrar factories de descriptores personalizados
    }
};

// Registro en el engine
entityManager->register_provider(new MyEntityProvider());
```

### 2. Pooling de Entidades

Para entidades con alta frecuencia de creación/destrucción (proyectiles, partículas):

```cpp
class PooledProjectileEntity : public Entity
{
    static std::vector<PooledProjectileEntity*> s_pool;
    
public:
    static PooledProjectileEntity* acquire() {
        if (s_pool.empty()) {
            return new PooledProjectileEntity();
        }
        auto* entity = s_pool.back();
        s_pool.pop_back();
        return entity;
    }
    
    void release() {
        // Reset state
        s_pool.push_back(this);
    }
};
```

### 3. Mixins con Componentes

Combina herencia de entidades con composición de componentes:

```cpp
class MovableEntity : public Entity
{
protected:
    PhysicsComponent* m_physics;
    
public:
    BasicTaskPtr initialize() override {
        m_physics = get_component<PhysicsComponent>();
        return TaskUtils::completed_task();
    }
    
    void move_to(const Vector3& position) {
        if (m_physics) m_physics->set_position(position);
    }
};

class EnemyEntity : public MovableEntity
{
    // Hereda funcionalidad de movimiento
    // Añade lógica específica de enemigo
};
```

## Conclusión

**No hay una única respuesta correcta**. La arquitectura de Pakal te permite elegir entre:

1. **Enfoque genérico (tipo Unity)**: `GenericEntity` + componentes + descriptores
   - Ventaja: Flexibilidad, rapidez de desarrollo
   - Desventaja: Menos type-safety, overhead de búsqueda

2. **Enfoque especializado (tipo Unreal)**: Herencia de `Entity`
   - Ventaja: Performance, APIs claras, type-safety
   - Desventaja: Menos flexible, más código

3. **Enfoque híbrido (recomendado)**: Combina ambos según necesidad
   - Usa genéricos para entidades comunes
   - Especializa para casos únicos o críticos

La clave es **comenzar genérico** y **especializar cuando sea necesario**, manteniendo el balance entre flexibilidad y performance según las necesidades específicas de tu juego.
