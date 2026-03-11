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

## Handoff Instructions for Local Coding Agent

This section is the implementation handoff to continue and finish the current ContraGame demo.

### Current Working Baseline (DO NOT BREAK)

- bgfx renderer initializes correctly on Linux with Vulkan.
- Sprites now render correctly (custom `vs_sprite/fs_sprite` path is active with fallback shaders).
- Title menu works (`GameTitleState`) and transitions into gameplay.
- Gameplay has 2-level progression (MVP) with level transitions and return to title.
- Build command works:

```bash
./docker-build.sh build runtime
```

### High-Priority Gaps to Finish the Game

1. **Projectile System is still stubbed**
    - `Weapon::fire()` only logs and does not create projectiles.
    - Implement real projectile entities (`sprite + physics + lifetime + owner/faction`).

2. **Damage Loop is incomplete**
    - Projectiles must apply damage to `Health` on collision.
    - Player bullets should damage enemies only.
    - Enemy bullets should damage player only.

3. **Enemy death handling is incomplete**
    - Dead enemies should become inactive/removed from update loops.
    - Level completion should be based on intended design:
      - Current MVP: reach goal X.
      - Suggested final demo: goal X + minimum enemy clear (or all enemies).

4. **Entity lifecycle needs cleanup policy**
    - Avoid leaks: when projectiles/enemies are removed, ensure components and entities are deleted consistently.
    - Keep cleanup localized to `GamePlayState` level ownership model.

### Recommended Implementation Order

#### Phase 1 — Real Projectiles
- Add a simple `Projectile` component (speed, direction, damage, faction, ttl).
- Spawn projectile entity in `Weapon::fire()` using `EntityManager`.
- Add sprite + physics body creation in one helper to keep behavior consistent.

#### Phase 2 — Collision + Damage
- Detect collision via physics callbacks or polling contact state.
- On hit:
  - Find `Health` on target entity.
  - Call `take_damage()`.
  - Destroy projectile entity.

#### Phase 3 — Win/Lose Rules
- Update `GamePlayState::check_win_lose_conditions()`:
  - Lose when player health <= 0 or fall out of world.
  - Win level when objective met (goal X + optional enemy clear).
  - Keep level transition delay small (0.8–1.2s).

#### Phase 4 — Feel/Polish (MVP)
- Tune movement/jump/fire-rate values.
- Add visible feedback on hit/death (color flash or quick fade).
- Keep logs useful but not noisy.

### Files to Prioritize

- `examples/ContraGame/Components/Weapon.h`
- `examples/ContraGame/Components/Weapon.cpp`
- `examples/ContraGame/Components/Health.h`
- `examples/ContraGame/Components/Health.cpp`
- `examples/ContraGame/GameStates/GamePlayState.h`
- `examples/ContraGame/GameStates/GamePlayState.cpp`

### Critical Constraints

- Do not regress sprite rendering pipeline.
- Do not remove existing title state and level transition flow.
- Keep compatibility with current bgfx+Box2D architecture.
- Prefer minimal, testable increments (build after each phase).

### Validation Checklist (must pass)

1. Build succeeds: `./docker-build.sh build runtime`
2. Title screen appears and transitions to gameplay.
3. Player can move, jump, and fire visible projectiles.
4. Enemy and player can both take damage and die.
5. Level 1 transitions to level 2.
6. Completing level 2 returns to title.
7. No immediate crashes when entering/exiting gameplay.

## License

Copyright (C) 2026 The PakalEngine Open Source Project
