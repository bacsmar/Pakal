# Contra Game Example - Implementation Guide

## Overview
This is a Contra-style side-scrolling run & gun game example for the Pakal Engine. This project demonstrates how to create a complete 2D action game using Pakal's entity-component system.

## Current Implementation Status

### ✅ Completed Components
- **Health System** (`Components/Health.h/cpp`)
  - Health/damage management
  - Invincibility support
  - Death events
  
- **Weapon System** (`Components/Weapon.h/cpp`)
  - Fire rate control
  - Projectile spawning (stub)
  - Direction-based shooting
  
- **Project Structure**
  - Directory layout created
  - Component organization established

### 🚧 In Progress / TODO

#### Phase 1: Graphics Backend (Required First)
The game requires a bgfx graphics backend which doesn't exist yet. Two options:

**Option A: Full bgfx Backend (Recommended for Production)**
Create a complete bgfx graphics system:
1. `source/bgfx/BgfxGraphicsSystem.h/cpp` - Main graphics system
2. `source/bgfx/SpriteComponent_Bgfx.h/cpp` - 2D sprite rendering
3. `source/bgfx/CameraComponent_Bgfx.h/cpp` - 2D camera
4. bgfx library integration
5. Shader compilation pipeline
6. Texture loading with bimg

**Option B: Use Existing Irrlicht Backend (Faster Prototyping)**
Adapt the game to use the existing `IrrGraphicsSystem`:
1. Use `SpriteComponent_Irrlicht` instead of bgfx sprites
2. Use `CameraComponent_Irrlicht` for camera
3. Modify shaders to work with Irrlicht's OpenGL/ES backend
4. Update Config.h to use `PAKAL_USE_IRRLICHT=1`

#### Phase 2: Game Components
- [ ] **PlayerController** (`Components/PlayerController.h/cpp`)
  - WASD/Arrow key movement
  - Jump mechanics
  - Shooting control
  - Animation state management
  
- [ ] **EnemyAI** (`Components/EnemyAI.h/cpp`)
  - Patrol behavior
  - Chase/Attack states
  - Line-of-sight detection
  - Weapon integration
  
- [ ] **Collector** (`Components/Collector.h/cpp`)
  - Collectible item detection
  - Score/health pickups

#### Phase 3: Game Systems
- [ ] **CollisionSystem** (`Systems/CollisionSystem.h/cpp`)
  - Physics collision callbacks
  - Layer-based filtering
  - Damage application
  
- [ ] **SpawnSystem** (`Systems/SpawnSystem.h/cpp`)
  - Enemy wave spawning
  - Collectible placement
  
- [ ] **ScoreSystem** (`Systems/ScoreSystem.h/cpp`)
  - Score tracking
  - Lives management
  - High score

#### Phase 4: Game States
- [ ] **GamePlayState** (`GameState/GamePlayState.h/cpp`)
  - Main game loop
  - Level creation
  - Win/lose conditions
  
- [ ] **MenuState** (`GameState/MenuState.h/cpp`)
  - Main menu UI
  - Start game
  
- [ ] **GameOverState** (`GameState/GameOverState.h/cpp`)
  - Game over screen
  - Restart option

#### Phase 5: Entity Definitions
- [ ] **Player** (`Entities/Player.h/cpp`)
  - Player entity setup
  - Component composition
  
- [ ] **Enemies** (`Entities/Enemy*.h/cpp`)
  - Soldier (ground enemy)
  - Turret (stationary)
  - Enemy factory

#### Phase 6: Application & Main
- [ ] **ContraGame** (`ContraGame.h/cpp`)
  - Application class
  - Resource loading
  - State registration
  
- [ ] **main.cpp**
  - Entry point
  - Engine initialization
  - Settings configuration

#### Phase 7: Assets & Shaders
- [ ] Sprite shaders (vs_sprite.sc, fs_sprite.sc)
- [ ] Placeholder sprite assets
- [ ] Background layers
- [ ] Sound effects (optional)

#### Phase 8: Build System
- [ ] CMakeLists.txt or VS project file
- [ ] Dependency configuration
- [ ] Shader build pipeline

## Quick Start (Once Complete)

### Prerequisites
- Pakal Engine with bgfx support enabled
- bgfx, bx, bimg libraries
- Box2D physics library
- SFML (for input)

### Building
```bash
# Enable bgfx in Config.h
# Set PAKAL_USE_BGFX=1

mkdir build
cd build
cmake ..
make ContraGame
./ContraGame
```

### Controls
- **Arrow Keys / WASD**: Move
- **Space**: Jump
- **X / Z**: Shoot
- **ESC**: Pause/Menu

## Architecture

### Entity-Component System
The game uses Pakal's ECS architecture:
```
Entity (Player, Enemy, Projectile)
  ├─ SpriteComponent (rendering)
  ├─ PhysicsComponent (Box2D body)
  ├─ Health (damage system)
  ├─ Weapon (shooting)
  └─ PlayerController/EnemyAI (behavior)
```

### Game Loop
```
1. Process Input (InputManager)
2. Update Components (PlayerController, EnemyAI, Weapon, Projectile)
3. Physics Step (Box2DPhysicsSystem)
4. Collision Detection (CollisionSystem)
5. Update Game Systems (SpawnSystem, ScoreSystem)
6. Render (SpriteComponent, CameraComponent)
7. UI Rendering (Health bar, Score)
```

### Physics Layers
- **Player**: Layer 1
- **Enemies**: Layer 2
- **Projectiles**: Layer 3 (no self-collision)
- **Platforms**: Layer 4 (static)
- **Collectibles**: Layer 5 (sensor)

## Design Decisions

### Why bgfx?
- Modern, cross-platform rendering backend
- Supports multiple graphics APIs (DX11, DX12, OpenGL, Vulkan, Metal)
- Explicit control over rendering pipeline
- Great for 2D sprite batching

### Sprite Rendering
- Uses transient vertex buffers for dynamic sprites
- Texture atlases for sprite sheets
- Z-ordering for layer management
- Optional sprite batching for performance

### Physics
- Box2D for rigid body physics
- Kinematic bodies for moving platforms
- Sensor bodies for collectibles
- Ray-casting for ground detection

### Performance Targets
- 60 FPS minimum
- Support for 50+ enemies on screen
- Smooth camera following
- No memory leaks

## Testing Checklist

Once implementation is complete:
- [ ] Player can move left/right smoothly
- [ ] Jump feels responsive
- [ ] Shooting direction is accurate
- [ ] Enemies patrol correctly
- [ ] Enemies chase and attack player
- [ ] Projectiles collide and damage correctly
- [ ] Health system works (take damage, heal, die)
- [ ] Score increases on enemy kill
- [ ] Lives system works correctly
- [ ] Game over appears when lives = 0
- [ ] Win condition triggers correctly
- [ ] Camera follows player smoothly
- [ ] Camera respects level boundaries
- [ ] No physics glitches
- [ ] Stable 60 FPS

## Known Limitations

- No audio system integration yet
- Placeholder graphics (colored rectangles)
- Single level only
- No power-ups or weapon upgrades
- No boss fights
- No save/load system

## Future Enhancements

- Multiple weapon types
- Power-ups (rapid fire, spread shot)
- Multiple levels with increasing difficulty
- Boss fights
- Cutscenes
- Parallax scrolling backgrounds
- Particle effects (explosions, muzzle flash)
- Network multiplayer
- Gamepad support
- Rebindable controls

## Contributing

When implementing missing components:
1. Follow Pakal's existing patterns (see Irrlicht components)
2. Use RTTI macros for component registration
3. Implement initialize/terminate lifecycle
4. Use Event<> for component communication
5. Add proper error handling and logging
6. Document public API
7. Test thoroughly

## References

- Pakal Engine: See `source/` for existing components
- bgfx: https://github.com/bkaradzic/bgfx
- Box2D: https://box2d.org/documentation/
- Contra (NES) game design reference

## License

Copyright (C) 2026 The PakalEngine Open Source Project
