# ADR-0019: Library Upgrades (Box2D 3, bgfx, the vcpkg Baseline)

Status: Accepted (2026-11-16). bgfx is done (2026-11-20); SDL3 and miniaudio (ADR-0018) and Box2D 3 follow.

## Context
Every library the engine uses was reviewed at once (2026-11-16):

| Library | Version | Where it comes from | Decision |
|---|---|---|---|
| SFML | 2.6.2 | vcpkg (override) | Replaced by SDL3 and miniaudio (ADR-0018) |
| Box2D | 2.4.1 | vcpkg (override); on Android, built from the folder where vcpkg extracted its source | **Box2D 3** (this ADR) |
| bgfx, bx, bimg | master of 2026-01 | git submodules | **Updated to the master of 2026-11-15** (done) |
| Dear ImGui | comes with bgfx's 3rdparty | bgfx | Updated along with bgfx |
| Assimp | 6.0.5 | vcpkg | Current; it moves with the baseline |
| Lua | 5.5.1 | vcpkg | Current |
| RmlUi | 6.3 | vcpkg | Current; it moves with the baseline |
| nlohmann-json | 3.12.0 | vcpkg | Current |
| FreeType, stb | 2.14.3 / 2026-08 | vcpkg | Current |
| ENet | 1.3.18 | git submodule | Latest release |

Only three libraries really fell behind: SFML, Box2D and bgfx.

Box2D 3 is a rewrite. Its API is C with handles (`b2WorldId`, `b2BodyId`, `b2ShapeId`) instead of classes. Its solver
works differently: a soft step with substeps. Contacts and sensors come as arrays of events instead of callbacks. It is
much faster, it can run on several threads, and since 3.1 it gives the same results on x64 and on ARM. It also has a
character mover.

In the engine, Box2D lives in `source/box2D` (about 16 files). Games reach it through `PhysicsWorld2D`, `PhysicsBody2D`
and `CharacterController2D`. A game whose characters are physics bodies (a fighting game, a platformer) moves
differently under a different solver, and anything trained or recorded on the old one (AI, replays) changes with it.

## Decision
1. **Box2D 3, after the 2D physics became an abstraction (ADR-0017).** The physics' public headers carry no Box2D type,
   and `<box2d/box2d.h>` appears only in `.cpp` files, so the upgrade rewrites only the inside of `source/box2D`, and no
   game changes.
   - The engine's API stays as it is. What Box2D 3 does differently (contact events, sensors) is absorbed inside the
     backend, and `PhysicsListener` and the queries behave the same.
   - The reasons, in order: the same results on x64 and ARM (AI trained on a desktop plays on ARM phones; it also helps
     replays and network play), performance (large crowds), and the character mover.
   - The upgrade is measured before it merges: the same inputs and seeds on both versions, and how much the movement
     changes.
   - Android builds Box2D from a pinned source: the vcpkg arm64-android package, or a submodule. It no longer builds
     from the folder vcpkg happened to extract its source into.
2. **bgfx, bx and bimg move to current master**, all three together, because they depend on each other. The shaders are
   compiled again with the new shaderc. Checked on the renderers in use: D3D11 on Windows, OpenGL and Vulkan on Linux,
   and GLES on Android.
3. **A new vcpkg baseline** for the rest, when one is needed. Desktop and Android use the same versions, and the
   arm64-android libraries are rebuilt from the same baseline. (2026-11-16: not needed yet. The baseline already has
   every library at its current version except Box2D, now 3.2.0, which is the version the Box2D upgrade targets.)
4. **Rules for every upgrade, now and later:**
   - one library per pull request;
   - every version pinned (a vcpkg override or a submodule commit), never "latest";
   - each pull request records ctest, the Android build, and what was checked by hand on desktop and a phone;
   - an upgrade that changes behavior says so, with what was measured.

## Order
1. The 2D physics as an abstraction (ADR-0017), then Box2D 3.
2. In parallel: ADR-0018's step 0 (closing the SFML leak), then SDL3 and miniaudio.
3. bgfx was done first of the three, since it touched neither physics nor audio, so a rendering problem was never mixed
   up with a physics or audio one.

## Notes on the bgfx update (2026-11-15 master)
- **Instance data moved.** bgfx now places instance data from `TEXCOORD31` downward
  (`BGFX_CONFIG_INSTANCE_DATA_FIRST_TEXCOORD`). Direct3D 11 binds attributes by semantic, so shaders must declare
  `i_data0..4` on `TEXCOORD31..27`; OpenGL binds by name and did not show the problem.
- **`bimg_decode` no longer carries miniz.** The engine links vcpkg's kubazip where Assimp brings it, and compiles bimg's
  `miniz.c` otherwise.
- **The OpenGL compute barrier fix** the engine carried as a patch is in bgfx itself.

## Consequences
- The libraries are current, and every version is pinned in one place.
- The physics abstraction stops being only cleanup: it is what makes the Box2D 3 change cheap.
- Games that depend on exact physics (trained AI, recorded replays) pay once when Box2D 3 lands.
- The Android build stops depending on vcpkg's internal folders.

## Related Documents
- ADR-0017 Games Never See the Backends, ADR-0018 SDL3 and miniaudio Replace SFML
