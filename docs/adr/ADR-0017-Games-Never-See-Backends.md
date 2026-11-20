# ADR-0017: Games Never See the Backends

Status: Accepted (2026-11-15). The code does not comply fully yet; see "Where the code stands".

## Context
The engine's abstractions (`GraphicsSystem`, `PhysicsSystem`, `SoundManager`, the abstract components) were made so
that a game never depends on how a system is implemented. That was the original intention of the engine. Over time the
3D renderer grew only in the bgfx implementation, and the games reached for it directly.

## Decision
A game module or a genre includes only the engine's abstractions. It never includes:

- a backend's classes: `source/bgfx/*` (`BgfxGraphicsSystem`, `MeshComponent3D_Bgfx`, `CameraComponent3D_Bgfx`...),
  `source/sfml/*`, `source/android/*`;
- a third-party library the engine is built on: `bgfx/`, `bx/`, `bimg/`, `box2d/`, `SFML/`, `RmlUi/`, `imgui/`.

What a game needs from a system is added to the abstraction (`GraphicsSystem` or an interface of its own), and the
backend implements it. A cast to the backend's class (`static_cast<BgfxGraphicsSystem*>`) is a missing abstraction.

These are not backends, and games may use them: the engine's services (`net/`, `scripting/`, `mcp/`), the gameplay
pieces (`gameplay/`), and the engine's 2D physics (`PhysicsWorld2D`, `PhysicsBody2D`, `CharacterController2D`,
`Box2DQueries`), whose API speaks the engine's own `Pakal::Vec2` and `Pakal::Aabb2`.

Only the platform layer (`android/main_android.cpp`, `apps/player`, `source/runtime/EngineBackends.cpp`) knows the
backends.

## Where the code stands (2026-11-20)
- **The 2D physics is an abstraction.** `Vec2` and `Aabb2` are the engine's own types, no physics header includes
  Box2D, and Box2D's include folders are PRIVATE on the engine's target.
- **SFML is private to the engine** (ADR-0018, step 0): no public header names an `sf::` type.
- **The guard is in place.** `cmake/CheckBackendIncludes.cmake` runs with every build and fails when a file of a game or
  a genre includes a backend header and is not listed in `cmake/backend_includes_pending.txt`. That list may only
  shrink.
- **The renderer is the gap.** `GraphicsSystem` has grown (quality levels, particles through `IParticleRenderer`,
  committing a frame's drawing, screenshots), but the 3D models and much of the look (shadows, post-process,
  lightmaps, light probes) are still reached through the bgfx classes. The files on the pending list are the TopDown
  genre's views and editor, the Zombies example, the brawler and dungeon examples, and the mesh viewer, which exercises
  the backend on purpose.

## Plan
1. Add the abstractions the games use. Per feature group, add a renderer interface (for example `IRenderer3D`, reached
   from `GraphicsSystem`) and an abstract `MeshComponent3D`, then make the bgfx classes implement them. No game changes
   in this step.
2. Move the games over, the genre first: TopDown, then the examples. Each move removes includes and casts, with no
   change in behavior, and takes its lines off the pending list.
3. Done: the 2D physics without Box2D types.
4. Done: the build guard.

## Consequences
- Games stay portable across backends and testable without them (`PAKAL_CORE_ONLY`).
- A packaged SDK, if one is made, would expose the abstractions only, with no backend classes or third-party headers.
- The abstractions grow with what the games need, so the renderer's features are designed as an API and not only as the
  bgfx class.
- Upgrading or replacing a backend (ADR-0018, ADR-0019) rewrites the backend's files, not the games.

## Related Documents
- ADR-0018 SDL3 and miniaudio Replace SFML, ADR-0019 Library Upgrades
