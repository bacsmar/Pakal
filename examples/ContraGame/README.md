# Contra Game Example

A side-scrolling run & gun game demonstrating Pakal Engine with bgfx + Box2D.

## Features

- **Player Movement**: Move left/right and jump with Box2D physics
- **Shooting System**: Fire projectiles in the direction you're facing
- **Enemy AI**: Enemies patrol, chase, and attack the player
- **Health System**: Damage and death mechanics for both player and enemies
- **Score & Lives**: Track your progress through the level
- **2D Rendering**: Sprite rendering using bgfx graphics backend
- **Camera System**: Smooth camera following with bounds

## Controls

- **Arrow Keys / WASD**: Move left/right
- **Space**: Jump
- **X / Z**: Shoot

## Building

Requires `PAKAL_USE_BGFX=1` and `PAKAL_USE_BOX2D=1` in `source/Config.h`

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make ContraGame
./bin/ContraGame
```

## Architecture

### Engine Systems
- **bgfx**: Modern cross-platform graphics rendering
- **Box2D**: 2D physics simulation
- **SFML**: Input handling and audio

### Rendering Components
- **SpriteComponent_Bgfx**: 2D sprite rendering with support for animation, scaling, rotation, and color tinting
- **CameraComponent_Bgfx**: 2D orthographic camera with smooth following and boundary constraints

### Game Components
- **PlayerController**: Handles player input, movement, and actions
- **EnemyAI**: Simple AI with patrol, chase, and attack behaviors
- **Health**: Damage and death system for entities
- **Weapon**: Shooting system with configurable fire rate and projectile speed

### Game States
- **GamePlayState**: Main gameplay loop, entity management, and game logic

## Code Structure

```
examples/ContraGame/
├── main.cpp                    # Entry point
├── ContraGame.h/cpp           # Main application class
├── CMakeLists.txt             # Build configuration
│
├── GameStates/
│   └── GamePlayState.h/cpp   # Main gameplay state
│
├── Components/
│   ├── Health.h/cpp          # Health/damage system
│   ├── Weapon.h/cpp          # Shooting system
│   ├── PlayerController.h/cpp # Player input/movement
│   └── EnemyAI.h/cpp         # Enemy AI behavior
│
└── Assets/
    └── sprites/              # Sprite images (placeholders)
```

## Game Design

### Player
- Controlled by keyboard input
- Can move left/right, jump, and shoot
- Has health and lives
- Blue colored sprite (placeholder)

### Enemies
- Patrol back and forth
- Chase player when in range
- Attack player when close enough
- Red colored sprites (placeholder)

### Level
- Ground platform at the bottom
- Several elevated platforms for jumping
- Camera follows player with bounds

### Win/Lose Conditions
- **Win**: Defeat all enemies
- **Lose**: Player dies with 0 lives remaining

## Extending the Game

This example can be extended with:

- **More Enemy Types**: Flying enemies, bosses, turrets
- **Power-ups**: Health packs, weapon upgrades, shields
- **Multiple Levels**: Level loading system, transitions
- **Better Graphics**: Replace placeholder sprites with actual art
- **Animation**: Add sprite sheet animations for characters
- **Sound Effects**: Gunfire, explosions, music
- **Particle Effects**: Muzzle flashes, explosions, dust
- **Advanced AI**: Flanking, cover usage, coordinated attacks
- **More Weapons**: Different bullet types, grenades, special attacks

## Technical Notes

### Component System
The game uses Pakal's Entity-Component-System (ECS) architecture:
- **Entities**: Game objects (player, enemies, platforms)
- **Components**: Reusable behaviors and data
- **Systems**: Update and render components each frame

### Physics Integration
Box2D handles:
- Collision detection
- Platformer physics (gravity, jumping)
- Projectile movement
- Ground detection

### Rendering Pipeline
bgfx provides:
- Hardware-accelerated 2D sprite rendering
- Texture management
- Shader-based rendering
- View/projection transforms

## Performance

Target: **60 FPS** at 1280x720 resolution

Optimizations:
- Sprite batching by layer
- Efficient collision detection
- Object pooling for projectiles
- Culling off-screen entities

## Known Limitations

This is a minimal example focused on demonstrating the engine's capabilities:

- Simple placeholder graphics (colored rectangles)
- Basic AI behaviors
- Single level
- No sound yet
- No menu system
- No save/load

## License

Copyright (C) 2026 The PakalEngine Open Source Project
