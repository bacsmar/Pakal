# AssetLoader - JSON-Based Entity Definition System

## Overview

`AssetLoader` is a Pakal engine component that loads game entities and components from JSON definition files. This allows you to declaratively define entities without hardcoding them in C++.

## Architecture

- **AssetLoader** (source/resources/AssetLoader.h/cpp): Main loader class that reads JSON and creates entities
- **SimpleJsonParser** (source/resources/SimpleJsonParser.h/cpp): Lightweight JSON parser with no external dependencies
- **JsonValue**: Simple tree-like JSON value representation

## JSON Format

```json
{
  "entities": {
    "player": {
      "type": "Pakal::GenericEntity",
      "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
      "components": [
        {
          "type": "SpritePhysicsComponent",
          "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
          "scale": 1.0,
          "body_type": "dynamic",
          "fixed_rotation": true,
          "gravity_scale": 1.0
        }
      ]
    }
  }
}
```

## Usage Example (ContraGame)

```cpp
#include "resources/AssetLoader.h"

// In your game state initialization:
Engine* engine = ...; // Your Pakal engine
AssetLoader assetLoader(engine);

// Load entities from JSON file
size_t loaded = assetLoader.load_from_file("Assets/entities.json");
LOG_INFO("Loaded %zu entities", loaded);

// Get loaded entity
GenericEntity* player = assetLoader.get_entity("player");
if (player) {
  auto* physicsComponent = player->get_component<SpritePhysicsComponent>();
  // ... use component
}

// Or retrieve all loaded entities
for (auto entity : assetLoader.get_loaded_entities()) {
  // Process entity
}
```

## Supported Components

- **SpritePhysicsComponent**: Animated sprite with physics
- **SpriteComponent**: Animated sprite without physics  
- **SpriteComponent2D**: Simple 2D sprite/texture rendering
- **CameraComponent2D**: 2D camera component

More components can be added by extending the `create_component_from_json()` method.

## Design Philosophy

1. **No external dependencies**: Uses only standard C++ (no nlohmann/json, rapidjson, etc.)
2. **Engine integration**: Part of Pakal source, reusable by any game
3. **Extensible**: Easy to add new component types
4. **Declarative**: JSON format is more readable than C++ code generation

## Future Enhancements

- [ ] Asset resource pooling/caching
- [ ] Nested entity hierarchies (parent-child relationships)
- [ ] Support for more component types
- [ ] JSON schema validation
- [ ] Hot-reloading of asset definitions
- [ ] Asset bundling for releases

## Files

- `source/resources/SimpleJsonParser.h/cpp` - JSON parsing
- `source/resources/AssetLoader.h/cpp` - Entity creation from JSON
- `examples/ContraGame/Assets/entities.json` - Example entity definitions

## See Also

- Pakal persist system (Archive/TextReader) for complex physics definitions
- EntityManager and ComponentManager for manual entity creation
