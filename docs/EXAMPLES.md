# Practical examples

1) Entity with Sprite + Physics

```c++
// se asume engine creado y m_entity_manager/m_component_manager inicializados
EntityHandle e = engine.entity_manager()->create();

// create a sprite component (Irrlicht)
auto spriteComp = engine.component_manager()->createComponent("SpriteComponent_Irrlicht", e);
// initialize with a sprite sheet / texture (pseudocode)
spriteComp->initialize({ .texture = "assets/player.png", .layer = 0 });

// create a physics component (Box2D)
auto physComp = engine.component_manager()->createComponent("SpritePhysicsComponent", e);
physComp->initialize({ .sprite_physics = spritePhysicsSettings, .position = {0,0,0} });

// visual/physics sync is now handled by the components themselves
```

2) Camera

```c++
// get the graphics system and create a camera
auto gs = engine.graphics_system();
// many implementations expose helpers to create camera nodes; pseudocode here:
CameraComponentSettings cs;
cs.position = {0,0,10};
engine.component_manager()->createComponent("CameraComponent_Irrlicht", entity)->initialize(cs);
```

3) Load a resource

```c++
ResourceManager::instance().add_source(std::make_shared<DirectorySource>("assets/"));
auto mesh = ResourceManager::instance().load<Mesh>("models/ship.obj");
```

4) Rocket GUI (create a simple document)

```c++
// in the GraphicsSystem initialization
rocket::Core::Context* ctx = /* obtener contexto */;
auto doc = ctx->CreateDocumentFromMemory("<div>Hello Pakal</div>");
doc->Show();
```

Notes: the examples mix pseudocode and real calls. Minor adjustments may be needed depending on the specific version of each component in the repo.
