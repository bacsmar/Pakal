# Zombies: Downtown level (new, additive level)

Downtown is a level made with the Quaternius CC0 packs and the game's gameplay logic. It is opened with
`run-zombies-downtown.bat`.

## How it is put together

It is **additive**: a large base (the city) and, on top, mission layers that only add things.

- **Base** (`tools/zombies_convert/downtown.py`): a 3x3 grid of intersections joined by 4-lane streets (12 m of asphalt and 3 m of
  sidewalk per side, at natural scale), four blocks with buildings in a pinwheel layout (5.5 m alleys leading to a courtyard), a ring of
  buildings on the outer streets and barricades (dumpsters) closing every street that leaves the map. The courtyard floor is made of 9 m asphalt
  slabs and the corner gaps are filled with sidewalk. Every building, barricade and street lamp has a collider built from its
  own footprint (the piece's geometry), without deforming anything.
- **Mission** (`tools/zombies_convert/mission_survival.py`): street cars as cover, cones, hydrants, bags, barrels, courtyard
  furniture, spawners behind the barricades, the gift chest and the script. Breakable props take their rules (health, drops, explosion)
  from the equivalent object in the original game.
- **Composition** (`build_downtown.py --mission survival_01`): joins base + mission and writes `Assets/zombies/downtown_<mission>/level.json`. The
  models are exported only once to `Assets/zombies/downtown/`; audio and shooting effects are shared with the original level.
  A new mission is a `mission_*.py` module with a `build(level, hollywood)` function that returns spawn, spawners, points, anchors and script.

## Characters

By default (`build_downtown.py --characters kit`) they are the ones from the Zombie Apocalypse kit: the hero Sam (the *SingleWeapon* version holds the pistol in the
left hand), `Zombie_Basic` for type 25001 and `Zombie_Chubby` for 25009 (one model per type, with their own pools), with the kit's clips
(`Idle_Gun`, `Run_Gun`, `Death`, `HitReact`, `Slash`, `Punch`...). The gift's melee weapons (axe, bats, guitar, spear) go in the right
hand (bone `Middle1.R`). Type 25001 is randomly dressed as `Zombie_Basic`, `Zombie_Arm` or `Zombie_Ribcage` (`visuals.enemy.<type>.variants`).
With `--characters mannequin` the Universal Animation Library mannequin is used instead: `merge_ual.py` merges its
85 clips into a single `.glb` and the player and the zombies are that mannequin tinted (blue, green, yellow).
Everything is declared in the `visuals` section of `level.json` (clips, height, color, model per enemy type, walk/run speed reference and
held objects), so any level can use other characters without touching code.

Engine: `MeshComponent3D_Bgfx` respects the base color of untextured glTFs, attaches a model to another one's bone (`attach_to`, resolved at draw time
so the weapon does not lag behind) and has per-instance opacity.

## Lighting and shading

All of it is in the engine (BgfxGraphicsSystem / MeshComponent3D_Bgfx) and is enabled per level in `level.json` (`lighting` and `post` sections); levels without
those sections (the original) look as before.

- **Light**: directional sun, sky ambient and ground bounce (hemispherical), soft wrap; sky colors.
- **Sun shadows**: 2048 depth map centered on what the camera looks at (buildings, props and characters cast; 3x3 PCF).
  If the sun is too low the streets between 28 m buildings end up in shadow: Downtown's sun is at ~63 degrees.
- **Baked GI** (`bake_gi.py`, run by `build_downtown.py`): a 97x97x4 grid of probes that cast 72 rays (sky, ground, buildings, with one
  sun bounce and its shadow). It is the real ambient light: dark in alleys and under walls, open in streets. `gi.json` + `gi.bin` next to the level;
  it is rebaked only if the boxes or the light change. Per-piece lightmap slabs are not used: the modular pieces share UVs.
- **Metal-rough PBR** for glTFs with a normal map and ORM (Downtown): relief on asphalt and sidewalks, GGX sun highlights, baked occlusion in
  the ambient. `alphaMode` is also read from glTFs (crosswalks, glass). The original's models do not change.
- **Post-processing** (`PostSettings`): the frame goes to a framebuffer with depth and a final pass applies color (exposure, contrast,
  saturation, tint, vignette), tilt-shift (blur at the top and bottom, miniature effect) and screen-space ambient occlusion.
  **Fog** (`post.fog`: `density` per meter, `start` meters, `max`, `sky`, `color`) uses the same pass's depth. It lerps
  toward the sky's horizon color (or its own) with the distance from the camera. That costs one exponential per pixel,
  so it stays on at every quality level. In a top-down view, a start a little before the ground keeps the roofs clear
  and hazes the streets, which reads as height. The sky, at the far plane, takes at most `sky`, the haze at the horizon
  in camera pans.
- Variables: `ZOMBIES_POST=off`, `ZOMBIES_SHADOWS=off`, `ZOMBIES_GI=off` for comparison.

## Occlusion

Buildings between the isometric camera and the player become transparent (opacity 0.16, with visible edges) and turn
solid again when they leave the line of sight (`ZombiesLevelState::update_occluders`, segment-box test in `MouseAim.h`).

## Verification

The bot (`ZOMBIES_AUTOPLAY=1`) plays the level to its end. Debugging utilities: `ZOMBIES_START=x,y`
(the player starts there), `ZOMBIES_CAM_AT=x,y`, `ZOMBIES_CLIP_TEST`, `ZOMBIES_SHOTS`.

## Missions

A mission adds props, spawners, the chest and a script to the base (`tools/zombies_convert/mission_*.py`); `build_downtown.py --mission <name>`
generates `downtown_<name>/level.json`:

- `survival_01`: waves shaped like the original's (push, pause until the street is cleared, horde, berserk) but twice the
  size: 80 kills, up to ~32 zombies alive at once (`enemyPool` 40 in the level).
- `survival_02` (`mission_dogs.py`): a different prop layout, **dogs** (type 25013: they run at the player, leap and bite once) and
  **dog walkers** (25012: they keep their distance and let a dog loose). Waves: men, first dogs, a pause, the pack with walkers, and a
  berserk finale; 40 kills. It is played with `ZOMBIES_LEVEL=...\downtown_survival_02\level.json`.

The special zombies (Boomer 25016, Tourist 25010, Zombinator 25011, DogWalker 25012, dog 25013) take their rules from the converted
Survival level and their looks from `special_looks` in `build_downtown.py`: a kit model, and the clips of their attack steps
(`actions`). The Boomer's throw is `OverhandThrow` of the Universal Animation Library, copied onto `Zombie_Chubby` with Blender.
`survival_01` brings them in little by little. A mission can still add a type of its own by returning it in `result['enemies']`
(rules and look object).

## Gift firearms

The original game only has the Glock; Downtown adds three firearms that the chest lends for a while (like the axe): SMG (15101),
Shotgun (15102, 7 pellets in 28 degrees) and Rifle (15103). They are `rules.objects` objects with a `FireArm` component (`FireRate`, `Magazine`,
`Rounds`, `ReloadTime`, `RunSpeed`, `FireSpeed`, `BulletSpeed`, `BulletDamage`, `Pellets`, `SpreadDegrees`, `FireSound`, `ReloadSound`) and a
`Market` with the `TIMER`; `build_downtown.py` (`FIREARMS`) adds them and distributes the chest (the axe comes up less often). Each weapon has its own
ammo while it lasts and when it ends the hero goes back to the Glock. Its kit model goes in the pistol hand (`KIT_FIREARM_POSE`: the model's pistol
pose rotated 90 degrees), so it points along the arm; the model's pistol stays inside it.
`ZOMBIES_GIVE=15102` gives it at the start (debugging).
