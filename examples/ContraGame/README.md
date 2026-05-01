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
- **Main Menu**: Title screen before gameplay
- **Two-Level Progression**: Complete level 1 to unlock level 2 in the same run

## Controls

- **Arrow Keys / WASD**: Move left/right
- **Space**: Jump
- **X / Z**: Shoot
- **Esc**: Return to title menu during gameplay

## Building

Requires `PAKAL_USE_BGFX=1` and `PAKAL_USE_BOX2D=1` in `source/Config.h`

```bash
./docker-build.sh build runtime debug
cd docker-build/bin
./PakalPlayer
```

ContraGame is built as a loadable module (`libContraGameModule.so`). `PakalPlayer` is the executable host and loads the module from the same directory by default.

## Editing Levels With Tiled

The runtime still loads Pakal scene JSON from `examples/ContraGame/Assets/scenes/`, but editable Tiled maps are generated in `examples/ContraGame/Assets/tiled/`.

Open the current levels in Tiled:

```bash
tiled examples/ContraGame/Assets/tiled/level1.tmj
tiled examples/ContraGame/Assets/tiled/level2.tmj
```

After editing and saving a `.tmj`, convert it back to the Pakal scene JSON before rebuilding or running:

```bash
python3 tools/pakal_tiled_scene.py tiled-to-pakal examples/ContraGame/Assets/tiled/level1.tmj examples/ContraGame/Assets/scenes/level1.json
python3 tools/pakal_tiled_scene.py tiled-to-pakal examples/ContraGame/Assets/tiled/level2.tmj examples/ContraGame/Assets/scenes/level2.json
```

To regenerate Tiled maps from the runtime JSON:

```bash
python3 tools/pakal_tiled_scene.py pakal-to-tiled examples/ContraGame/Assets/scenes/level1.json examples/ContraGame/Assets/tiled/level1.tmj
python3 tools/pakal_tiled_scene.py pakal-to-tiled examples/ContraGame/Assets/scenes/level2.json examples/ContraGame/Assets/tiled/level2.tmj
```

Keep `player`, `goal`, and `camera` as object names. Enemy object names should include `enemy`, such as `enemy_01`, because the gameplay state uses those names to wire gameplay components.

## Architecture

### Engine Systems
- **bgfx**: Modern cross-platform graphics rendering
- **Box2D**: 2D physics simulation
- **SFML**: Input handling and audio

### Rendering Components
- **SpriteComponent2D**: Backend-agnostic 2D sprite wrapper for animation, scaling, rotation, and color tinting
- **CameraComponent2D**: Backend-agnostic 2D orthographic camera with smooth following and boundary constraints

### Game Components
- **PlayerController**: Handles player input, movement, and actions
- **EnemyAI**: Simple AI with patrol, chase, and attack behaviors
- **Health**: Damage and death system for entities
- **Weapon**: Shooting system with configurable fire rate and projectile speed

### Game States
- **GameTitleState**: Main menu/title screen
- **GamePlayState**: Main gameplay loop, level transitions, and game logic

## Code Structure

```
examples/ContraGame/
├── main.cpp                    # Entry point
├── ContraGame.h/cpp           # Main application class
├── CMakeLists.txt             # Build configuration
│
├── GameStates/
│   ├── GameTitleState.h/cpp  # Main menu / title screen
│   └── GamePlayState.h/cpp   # Gameplay + level flow
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
- Two distinct side-scrolling levels
- Ground and elevated platforms for traversal
- Camera follows player with level bounds

### Win/Lose Conditions
- **Level Complete**: Reach the right-side objective of each level
- **Game Complete**: Finish level 2 and return to title
- **Lose**: Fall out of the map or run out of lives

## Extending the Game

This example can be extended with:

- **More Enemy Types**: Flying enemies, bosses, turrets
- **Power-ups**: Health packs, weapon upgrades, shields
- **More Levels**: Extend current level progression system
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
- Two levels (MVP progression)
- No sound yet
- Basic title menu only
- No save/load

---

## Current Implementation Status

ContraGame is no longer a compile-only stub. The current demo includes:

- Title screen with transition into gameplay
- Two scene-driven levels with cleanup between transitions
- Player movement, jumping, and shooting
- Enemy patrol/chase/attack behavior
- Real projectile entities with sprite, physics, lifetime, faction, and collision damage
- Deferred entity disposal for projectiles, dead enemies, title entities, and level entities
- Camera configuration from scene JSON, including per-level zoom

### Remaining Polish

- Tune movement, jumping, projectile speed, and enemy fire rate after hands-on playtesting.
- Replace simple ground detection with a Box2D raycast or foot sensor.
- Replace distance-only enemy sight checks with raycast-based line of sight.
- Add hit/death feedback, score/lives UI, sound effects, and music.
- Decide whether level completion should require enemy clear in addition to reaching the goal.

### Validation Checklist

1. Build succeeds: `./docker-build.sh build runtime debug`
2. Title screen appears and transitions to gameplay.
3. Player can move, jump, and fire visible projectiles.
4. Player projectiles damage enemies only.
5. Enemy projectiles damage the player only.
6. Level 1 transitions to level 2.
7. Completing level 2 returns to title.
8. Re-entering gameplay does not keep old level entities alive.

## License

Copyright (C) 2026 The PakalEngine Open Source Project
