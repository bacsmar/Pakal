# Particle Effects

An effect is a JSON file, not code. A game plays it at a point of the floor and gets back a handle to move or stop it.
The code is `game_genres/topdown/view/ParticleEffects` (TopDown games, through `LevelRuntime::particles()`). It draws
through `IParticleRenderer` (`source/IParticleRenderer.h`), so the game and the genre see no backend.

## Playing one

```cpp
auto& fx = m_runtime->particles();                       // made by LevelRuntime::begin
fx.prefetch({ "Assets/mygame/vfx/blood_hit.json" });       // any thread: read once, kept
auto h = fx.play("Assets/mygame/vfx/fire_loop.json", at, 0.0f, direction, 1.0f);
fx.set_position(h, carAt, 0.5f, carHeading);               // each frame, for a fire on a moving car
fx.stop(h);                                                // no more particles; stop(h, true) also clears the live ones
fx.update(gameSeconds);                                    // each frame, with the game's own seconds (0 while paused)
```

- Points are in the game's frame: x and y on the floor, height up. `place_level`'s origin and yaw are already applied.
- Particles that land stand on `place_level`'s floor heights.
- `LevelRuntime::end()` clears everything.
- `load_sheet("Assets/mygame/vfx/sheet.json")` is a graphics-thread call. It sets the sprite sheet, and emitters then
  name their sprites by it. Call it before the effects that use sprites are read. Without a sheet, particles are soft
  dots.

## The file

```json
{ "emitters": [ { "shape": "cone", "angle": 35, "burst": 18, "life": [0.5, 0.9], "speed": [2, 4.5], "size": [0.03, 0.06],
                  "gravity": 9.8, "color": [[0, 0.55, 0.02, 0.02, 1], [1, 0.35, 0, 0, 0.9]], "floor": "stay",
                  "decal": { "size": [0.08, 0.18], "seconds": 8, "color": [0.3, 0, 0, 0.85] } } ] }
```

Each emitter (all fields optional):

| Field | Meaning | Default |
|---|---|---|
| `shape` | `point`, `sphere` (`radius`), `cone` (`angle` half-opening in degrees, `elevation` degrees over the play direction, `radius` of its base), `box` (`box`: half sizes along, across, up) | `point` |
| `burst` | particles at the start | 0 |
| `rate`, `duration`, `loop`, `delay` | particles a second for `duration` seconds (or forever with `loop`), after `delay` | 0, 1 with a rate, false, 0 |
| `max` | live particles of this emitter in one effect | 256 |
| `life`, `speed`, `size` | seconds, meters/s, meters; a number or `[min, max]` | 0.5–1, 1–2, 0.1–0.2 |
| `rotation`, `spin` | degrees, and degrees/s | 0 |
| `height` | start height over the effect's | 0 |
| `gravity`, `drag` | meters/s² down (negative rises); share of the speed lost each second | 0, 0 |
| `color` | `[r, g, b]`, `[r, g, b, a]`, or stops over life `[[t, r, g, b, a], ...]` | white |
| `sizeOverLife` | a number, or points `[[t, factor], ...]` | 1 |
| `blend` | `alpha` or `add` | `alpha` |
| `sprite`, `frames`, `fps` | the sheet's cell (a name from the sheet or a number from 1), how many cells it plays (a flipbook), at `fps` or spread over its life when 0 | soft dot |
| `floor` | `none`, `stay` (lands and stays), `bounce` (`restitution`, `friction`) | `none` |
| `decal` | a stain where a particle first lands: `sprite`, `size`, `seconds`, `fade`, `color`, `blend` | none |
| `subEmitters` | `{ "death": ..., "land": ... }`: another effect (a file next to this one, or an emitter written in place) started where a particle dies or first lands | none |
| `gpu` | `auto` or `never` | `auto` |

A file can hold up to 4096 live particles and 256 stains in all. Past that, the oldest stain gives way.

## CPU and GPU

Every emitter runs on the CPU. With `gpu: "auto"`, an emitter goes to the GPU's particles instead when both of these
hold:
- The device has compute: D3D11, Vulkan, or GLES 3.1. On GLES 3.0 phones the same file runs on the CPU.
- The emitter only uses what those particles do: one color whose alpha changes, a size from start to end, gravity, no
  drag, no sprite, a floor (`stay` or `bounce`), no decal and no sub-emitters.

The GPU's floor is flat at height 0, so on a level with heights use `gpu: "never"` for anything that must land on
them.

## Examples

`examples/brawler_proto/data/vfx` has six examples: `blood_hit`, `sparks_hit`, `dust_fall`, `debris_wood`,
`fire_loop` and `explosion`. In the brawler prototype:
- the hero's hits play blood and sparks;
- `BRAWLER_VFX=<name>` plays one effect in the middle of the arena.
