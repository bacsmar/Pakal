# Android build (Zombies, the brawler, the dungeon)

One `arm64-v8a` APK per game, each its own app (its package and its name on the launcher), so all of them are installed side
by side:

| Game | APK | Package | Library |
|---|---|---|---|
| Zombies (the Downtown levels) | `pakal-zombies.apk` | `org.pakalengine.zombies` | `libpakal_zombies.so` |
| The brawler prototype | `pakal-brawler.apk` | `org.pakalengine.brawler` | `libpakal_brawler.so` |
| The dungeon prototype, alone or online | `pakal-dungeon.apk` | `org.pakalengine.dungeon` | `libpakal_dungeon.so` |

Everything is built from source with the NDK. The engine, the TopDown genre and the Android entry point are compiled once
(`pakal_core`, an object library); each game's shared library takes all of it and adds its module. A `NativeActivity`
loads it. Only the dungeon takes the network (`pakal_net`: ENet, `source/net`, `game_genres/topdown/net`).

## How it is built

```
vcpkg install --triplet arm64-android --x-manifest-root=android/vcpkg --x-install-root=build-android/vcpkg_installed
python android/build_apk.py                  # every game: builds, packs, aligns (16 KB) and signs (debug key)
python android/build_apk.py --game dungeon   # one game
python android/build_apk.py --game <game> --cheats    # with the game's cheats, for testers (never a release)
```

A game kept outside the engine (in `games/<name>/`) builds the same way. Its folder says what it is:
- `android.json`: package, label, library, permissions, and optionally an `icon` and a `stager` (files of its folder).
  `"staging": "zombies"` packs it as the Zombies module does, with its own `Assets/<name>`. A `stager` is a Python file
  with `stage(stage, raw_textures, apk)` that packs it its own way, using `build_apk`'s helpers (`apk`).
- `android.cmake`: its library, built with `pakal_game` (android/CMakeLists.txt includes it, `GAME_DIR` set to its
  folder). `PAKAL_GAME_CHEATS` (`--cheats`) asks for its cheats: the game turns it into its own define.

It needs NDK r27, the SDK (build-tools, platform), JDK 17 and the SDK's cmake/ninja: point `ANDROID_NDK`, `ANDROID_SDK`
and `JAVA_HOME` to them, or write those three in `android/local_paths.json` (a file Git ignores).
The games' data is taken from `build-win64/bin/Assets`: build the desktop first, and for Zombies run `build_downtown.py`.
The brawler and the dungeon take their folder (`Assets/brawler`, `Assets/dungeon`), the interface's fonts and styles
(`Assets/ui`), and only the Zombies example's models that their rules name. `android/AndroidManifest.xml` is a template:
`build_apk.py` fills in each game's package, name, library and permissions (the dungeon asks for the network and for the
Wi-Fi's broadcasts). Output: `build-android/pakal-<game>.apk`.

## What changes compared to Windows

- `android/CMakeLists.txt` builds bx/bimg/bgfx (OpenGL ES 3.1, for compute shaders: a phone with only 3.0 does not run it; no Vulkan), Box2D 2.4.1 (the sources vcpkg extracted: its
  port tries to build glfw/X11 and fails on Android), the engine, the module and `android/main_android.cpp`.
- `PAKAL_NO_SFML` (Config.h): no SFML; the window, input and sound are provided by the platform. `PAKAL_STATIC_MODULE`
  (GameModuleLoader): the module is linked in, not loaded with `dlopen`.
- `WindowArgs.native_window` carries the `ANativeWindow*` to bgfx. On Android the default renderer is OpenGL ES.
- Shaders: the same `.sc` files, compiled with `--platform android -p 300_es` to `Assets/shaders/essl`.
- Touch input (`main_android.cpp`): left half = virtual stick, right half = aim and shoot. A touch that lands on a
  button of the game's interface (RmlUi: pause, the pause panel) or on a development panel (Dear ImGui) goes to it
  instead; three fingers at once show or hide the development panels. A game's own buttons are buttons of its
  interface that hold the button a moment (`InputMapper::tap`): the dungeon's abilities (in the bottom right corner,
  aimed at the nearest monster: it has no aim stick) and its bag,
  the brawler's HIT, HEAVY, JUMP, DODGE and GUARD. A game turns the sticks on while it is played
  (`IInputManager::set_touch_controls`); one that does not aim (the brawler) turns the right half's stick off
  (`set_touch_aim`), where its buttons are.
- A game can read the sticks itself instead (`IInputManager::set_game_touches`): every finger off the interface then
  goes to `event_touch_began/moved/ended` (screen pixels, `get_touch_area` is the screen's size) and the platform
  neither reads nor draws its sticks. TopDown games do this: `InputMapper::touch_sticks` turns the fingers into the
  move and aim sticks, and `TouchSticksPanel` draws them with an RmlUi document of the game's (its elements
  `move_base`, `move_thumb`, `aim_base`, `aim_thumb`; the body's classes `moving`, `aiming`, `firing`).
- The game reads its assets straight out of the APK through the resource manager (`AndroidAssetSource`). Nothing is
  copied to the app's storage. `PAKAL_EXTRACT=1` copies them as before (`assets.list` is the content version). Copies
  left by an older build are removed once.
- `build_apk.py` resamples the sounds to 22 kHz (`PAKAL_APK_FULL_AUDIO=1` keeps them as they are).

## Runtime

- `python android/build_apk.py` puts the levels in the APK; the game opens on its menu (the missions). `ZOMBIES_LEVEL` in
  `pakal_env.txt` (a full path) plays one level on its own instead.
- Debugging without a shell: whatever is put in `/sdcard/Android/data/<package>/files/pakal_env.txt` (`KEY=VALUE`
  per line) becomes an environment variable: `ZOMBIES_POST/SHADOWS/GI=off`, `ZOMBIES_AO`, `ZOMBIES_TILT`, `PAKAL_FPS_LOG=1`,
  `PAKAL_RENDER_HEIGHT`. With `PAKAL_EXTRACT=1`, assets are extracted again only if their hash changes (`assets.list`).
- The picture is drawn smaller than the screen (720 px high, 1080 at Ultra: `PAKAL_RENDER_HEIGHT`) and scaled up to the
  screen with FSR 1 (EASU rebuilds the edges, RCAS sharpens); the interface is drawn after it, at the screen's size, so its
  text stays sharp. The game's OPTIONS (menu and pause panel, saved in the profile) turn FSR off: the picture is then drawn
  at the screen's height up to the quality's cap (720, 1080 at Ultra) and stretched plainly. `PAKAL_FSR=0` leaves the scaling to the compositor (bilinear, the interface included); it wins over the game's saved FSR option for that launch. The screen's
  size is the window's (it extends under the navigation bar), not the content rect.
- Touch: the sticks and the aim work in the picture's pixels, the interface in the screen's. A menu touch that moves less
  than 3.5% of the screen's height is a tap where it started, and one that lands that close to a button takes the nearest
  one; the development panels are bigger on Android and take touches a little around each control.

## Quality levels

Each level raises samples and resolution over the one before (`BgfxGraphicsSystem::Quality`):

| Level | Shadows | Post-processing (ambient occlusion, blur) | Render height |
|---|---|---|---|
| Minimum | 2048 map | 30 % of the samples | 720 px |
| Medium | 2048 map | 50 % | 720 px |
| High | what the level asks for | all | 720 px |
| Ultra | what the level asks for | all | 1080 px |

At Minimum, a level with a lightmap keeps only what moves in the shadow map (1024): the fixed scene's shadow on the floor
is baked. `PAKAL_QUALITY=low|medium|high|ultra` sets it and `PAKAL_RENDER_HEIGHT` changes the height. Otherwise, on PC it is Ultra and on Android
it depends on the OS version: Android 8-9 Minimum, 10-13 High, 14 or later Ultra.

What keeps the cost down:
- Zombies stored in the pool (scale 0) are neither drawn nor animated.
- The shadow map covers the area around the camera, and buildings cast with simplified shadow casters.
- A faded building (between the camera and the hero) writes its depth first, so only its nearest layer is painted.
- Compressed textures with mipmaps (see below).

Movement looks smooth even though logic and rendering do not run at the same rate: the simulation advances in fixed steps of
1/60 s and characters and the camera are drawn interpolated between the previous step and the next one (`Actor::drawn_position`);
time is measured with a high-resolution clock and on Android the logic loop runs at 120 Hz.

`PAKAL_FPS_LOG=1` logs the models and triangles of each pass every second (`[GPU]`).

## APK textures

`build_apk.py` converts each PNG/TGA to KTX with ASTC 4x4 and mipmaps (`android/pack_textures.py`, with bgfx's `texturecRelease.exe`:
`external/bgfx/.build/projects/vs2022/texturec.vcxproj`, Release x64, toolset v145). It caps them at 1024 (at 720 px of
height from the overhead camera the 2048 level is almost never used) and removes the original. The engine loads `<file>.ktx` instead
of the file the model names. Normal and ORM maps are stored as linear data. There is a content-based cache in
`build-android/texture_cache`. `--raw-textures` keeps the originals.

- APK from 247 MB to 118 MB; on the GPU each 2048 texture went from ~21 MB (RGBA8 with mips) to ~1.3 MB (1024, ASTC 4x4).
- ASTC 6x6 does not work: 1024 is not a multiple of 6, the tool pads to 1026 and the image comes out shifted diagonally.
- texturec's `-n` option (normal map) stores a different encoding (without z) that the shaders do not read: dark blotches appear.
- When extracting (`PAKAL_EXTRACT=1`), the game deletes the files from the previous extraction that the package no longer
  contains.

## Orientation

If the game starts with the phone locked, the window is created in portrait. `main_android.cpp` takes the screen size from
`contentRect` (not from `ANativeWindow_getWidth`, which after `setBuffersGeometry` returns the buffer size) and, when the window changes shape,
redoes the geometry and hands the surface to bgfx again (which clears the debug text from the previous size).

## What failed on OpenGL ES (now fixed)

- The camera had a fixed 1280x720 viewport: the game appeared in a rectangle; now it follows the window size.
- Post-processing came out upside down and with stripes: OpenGL render textures start at the bottom (the shader assumed the top) and
  OpenGL ES samplers are `lowp` by default (depth, normals, shadows and light volume are now `highp`,
  `pakal_precision.sh`).
- Characters (skeletal) were not drawn: the array of 128 bone matrices (`u_model[128]`) does not work on Adreno;
  with 64 it does (the models use 41-49).
- Without a light volume (original level) nothing was drawn: a 3D sampler with no texture bound cancels the draw on Adreno; now
  a 1x1x1 volume is bound.
- Performance: static meshes outside the camera (and the shadow volume) are no longer drawn (`set_cull_frustum`).
  A camera's draw distance by size (`MeshComponent3D_Bgfx::set_draw_distance(perMeter, minimum)`): a model farther than
  its size times `perMeter`, and than `minimum`, is left out (a low camera shot sees the whole level; the top-down view never
  reaches it). A game sets it from its data (for example its effects' "drawDistance"); a camera shot's `far` (camera_shots.json, per
  quality) ends the view sooner, with the horizon's fog at that edge.

## The window at the start

A phone that locks while the game starts takes its window away before bgfx gets it: the game waits until there is one
again (`NativeWindowManager::setup_window`), with the shape it comes back with. Before, bgfx's render thread used the
window that was gone and the game crashed in `ANativeWindow_setBuffersGeometry`.

## The governor

When a phone cannot hold the frame rate (three seconds under 87% of it, or hot), `BgfxGraphicsSystem` steps down, one
step at a time: first the picture is stretched plainly instead of by FSR (a little sharpness, far cheaper on a phone's
GPU), then 30 FPS, then a smaller picture. With room to spare it steps back up, and a step up that does not hold makes
it wait longer before the next try.
- **What doesn't count:** the first six seconds after a level loads (a phone's driver draws a new level slowly at first),
  and any time the game holds it (`hold_governor(true)`), e.g. while its camera pans over a whole town.
- **Turning it off:** `PAKAL_MAX_FPS` fixes the rate and turns it off; `PAKAL_GOVERNOR=0` turns it off too.

## Sound, lifecycle and controls

- **Sound**: `android/SoundManagerAAudio` mixes the WAV effects (16-bit) by hand into a single AAudio stream; `AudioCues` uses it instead
  of SFML. The `.wav` files go in the APK. The original's music (`PlayMusic`) has no file yet.
- **Background**: when going to the background (or losing the window) the engine pauses; on return, bgfx receives the new window
  (`BgfxGraphicsSystem::set_native_window`) and the engine continues where it was. Verified with HOME and reopening.
- **Touch controls**: two analog sticks. The left half moves and the right half aims;
  each stick appears where the thumb lands (120 px radius on a 1080-high screen, 15 % dead zone). The aim
  stick fires past 41 % (its disc turns red); without it, the hero faces where it walks. The screen is the engine's
  axis device 0 (`IAxisDevice`: axes 0-1 move, 2-3 aim), so a gamepad could use the same thing. Restarting is in
  the pause panel (the button at the top right). The platform draws them with `BgfxGraphicsSystem::draw_screen_line`, unless the game reads the fingers itself (see
  `set_game_touches` above). `adb shell input swipe` drives them
  (in the coordinates of the phone's current resolution, not its panel's).
- **Movement** (same on PC): any tilt walks at full speed, starting accelerates (63 % in 0.1 s), on
  release it stops dead; the camera follows the hero with a short lag (SmoothDamp of 0.15 s) and the mouse does not turn it within
  0.5 m around it.
