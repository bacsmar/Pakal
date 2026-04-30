# ContraGame Implementation Status

## Current Baseline

ContraGame now builds as `libContraGameModule.so` and runs through the generic `PakalPlayer` host. The current build command is:

```bash
./docker-build.sh build runtime debug
```

Runtime from the build output directory:

```bash
cd docker-build/bin
./PakalPlayer
```

## Completed

- bgfx sprite rendering is active through the backend-agnostic `SpriteComponent2D` path.
- Box2D sprite physics initialization uses `SpritePhysicsComponent::Settings`.
- `GameTitleState` creates and disposes title entities through `EntityHandle`.
- `GamePlayState` loads two JSON scenes, tracks scene entities by handle, and cleans them up between level transitions.
- Camera zoom, viewport, position, and bounds can be loaded from scene JSON.
- Camera follow targets use `EntityHandle`, so level transitions do not retain stale entity pointers.
- `EntityManager` owns live entities, resolves `EntityHandle`, and processes deferred disposal.
- `Component::prepare_dispose()` lets physics/audio components tear down their own subsystem resources.
- `AssetLoader` and `SceneLoader` store handles internally and expose handle-first APIs.
- Player, enemy, weapon, and projectile components are updated each gameplay frame.
- `Weapon::fire()` creates projectile entities with physics, sprite, damage, lifetime, and faction.
- Projectiles subscribe to physics collision events and damage only valid opposing factions.
- Dead enemies are requested for disposal; player death and falling out of world trigger game over.

## Remaining MVP Polish

- Runtime needs one more hands-on pass for feel: movement, jump force, enemy fire rate, and projectile speed.
- Level completion is still goal-based. Enemy-clear completion can be added if the demo should require combat mastery.
- Ground detection is still a simple velocity heuristic rather than a Box2D raycast/contact-foot sensor.
- Enemy line of sight is distance-only and does not raycast through level geometry.
- There is no hit flash, death animation, score UI, lives UI, or sound pass yet.
- Projectiles are created/destroyed directly; object pooling is still a future performance improvement.

## Validation Checklist

- Build succeeds with `./docker-build.sh build runtime debug`.
- Title screen appears and transitions into gameplay.
- Player can move, jump, and fire visible projectiles.
- Enemy projectiles can damage the player; player projectiles can damage enemies.
- Dead enemies stop updating and are cleaned up by deferred disposal.
- Level 1 transitions to level 2; completing level 2 returns to title.
- Entering/exiting gameplay repeatedly does not leak old level entities or crash.

## Completion Estimate

- Engine/lifecycle architecture: 100%
- Scene loading and camera config: 100%
- Core gameplay loop: 90%
- Projectile and damage loop: 85%
- Visual/audio polish: 35%
- Documentation accuracy: 90%

Remaining work is mostly tuning, feedback, and a runtime playtest pass rather than API compatibility or compilation repair.
