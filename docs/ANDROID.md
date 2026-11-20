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
python android/build_apk.py --game <game> --side      # a tester's copy beside the store's (package .dev, name (dev))
```

A copy signed with the debug key cannot replace the one installed from the store (Play signs with its own key): `--side`
gives it another package (`<package>.dev`) and name (`<label> (dev)`), so both are installed at once, each with its own saved
games. Not with `--release` or `--bundle`.

A game kept outside the engine (in `games/<name>/`) builds the same way. Its folder says what it is:
- `android.json`: package, label, library, permissions, and optionally an `icon` and a `stager` (files of its folder).
  `"staging": "zombies"` packs it as the Zombies module does, with its own `Assets/<name>`. A `stager` is a Python file
  with `stage(stage, raw_textures, apk)` that packs it its own way, using `build_apk`'s helpers (`apk`).
  `"desktop_data"` names the folder of its own that its `pakal_add_game_module` copies to `Assets/<name>` (`"data"`
  usually). The APK packs the desktop build's copy, so the build stops if that copy differs from the folder, for
  example when it dates from before the last commit or from another branch. Build the desktop again, or set
  `PAKAL_APK_STALE_DATA=1` to pack it anyway.
  `"env"` (`{"KEY": "VALUE"}`) is the app's environment by default: it goes in the APK as `pakal_env.txt`, read before
  the device's own (which wins). A 2D game sets `"PAKAL_NO_3D": "1"`: the graphics system does not make the 3D programs
  (meshes, skinning, PBR, sky, particles, shadows), which a phone's driver otherwise compiles in the first frame (6.6 s on a
  a 2016 phone, 0.4 s without them). The desktop player reads the same `pakal_env.txt` beside it.
- `android.cmake`: its library, built with `pakal_game` (android/CMakeLists.txt includes it, `GAME_DIR` set to its
  folder). `PAKAL_GAME_CHEATS` (`--cheats`) asks for its cheats: the game turns it into its own define.

It needs NDK r27, the SDK (build-tools, platform), JDK 17 and the SDK's cmake/ninja: point `ANDROID_NDK`, `ANDROID_SDK`
and `JAVA_HOME` to them, or write those three in `android/local_paths.json` (a file Git ignores).
The games' data is taken from `build-win64/bin/Assets`: build the desktop first, and for Zombies run `build_downtown.py`.
The brawler and the dungeon take their folder (`Assets/brawler`, `Assets/dungeon`), the interface's fonts and styles
(`Assets/ui`), and only the Zombies example's models that their rules name. `android/AndroidManifest.xml` is a template:
`build_apk.py` fills in each game's package, name, library and permissions (the dungeon asks for the network and for the
Wi-Fi's broadcasts). Output: `build-android/pakal-<game>.apk`. The apps target API 36 (what Google Play asks of a new app);
the manifest keeps `enableOnBackInvokedCallback="false"`, so the Back key still reaches the game as a key event.

### Flavors and bundles for Google Play

```
python android/build_apk.py --game <game> --flavor demo --define NAME=VALUE ... --release                       # an APK
python android/build_apk.py --game <game> --flavor demo --define ... --bundle --release [--install-bundle SERIAL]
```

- **A flavor** is the game as another app, from its `android.json` `"flavors": {"<name>": {...}}`:
  - `package` and `label`: the app it becomes;
  - `defines`: its settings, and `required`, the names that must come with `--define`;
  - `signers`: the SHA-256 of the certificates its copies may carry: the upload key's and the store's app signing key's.
    `--signer` adds more, and a flavor with none is refused.
  - No key is opened to learn them: a certificate's SHA-256 is public (`keytool -printcert -jarfile` on a package it
    signed).
- **pakal_flavor.h:** `build_apk.py` writes the settings to `build-android/flavor/pakal_flavor.h`, one `#define` each.
  - It adds `PAKAL_FLAVOR_NAME` and `PAKAL_APP_SIGNERS`, the signers separated by commas.
  - The entry point and the game's library see the header (`PAKAL_FLAVOR` defined, `PAKAL_FLAVOR_DIR` in CMake).
  - A build without a flavor clears it.
  - `PAKAL_LOCKED` keeps the entry point from reading the device's `pakal_env.txt` (the external files folder), so nobody
    can turn on debug switches; the package's own defaults (`android.json` `"env"`, `PAKAL_RELEASE`) are still read.
    Every `--release` is locked, flavor or not, and builds its own native code (`--skip-native` is refused with it).
- **A flavor is a release:** it needs `--release`, and is signed with the upload key from the environment (below). It
  carries no cheats or tools, and is no `--side` copy.
- **`OSManager::app_signers()`** returns the certificates the installed app is signed with (SHA-256, lowercase hex).
  `PakalActivity.signerDigests` reports them at start. A game compares them with `PAKAL_APP_SIGNERS` to tell its own copy
  from one someone signed again.
- **Names:** `build-android/pakal-<game>[-<flavor>].apk`, or `pakal-<game>[-<flavor>]-<version>.aab`. Next to each one,
  `<package>.build.json` holds the build's identity (its number, commit, branch, version, code). The name doesn't say
  which build it is, since every build of a release has the same version.
- **`"asset_pack": true`** in a game's `android.json` puts its assets in an install-time asset pack, `gamedata`, instead of
  the bundle's base module. Google Play caps the base download at 200 MB. The pack installs with the app, and the game
  reads its files as the APK's own assets.
- **`--install-bundle SERIAL`** installs the bundle on a connected phone the way Play would: bundletool `build-apks
  --connected-device`, signed with the bundle's key, then `install-apks`. bundletool takes a password only on its command
  line (which other processes can read) or in a file. For a release, the passwords typed at the start go in files in a
  private folder of the run (`pakal-sign-*` in the user's temporary folder). The folder is deleted as soon as
  `build-apks` ends, whether it worked or not. A run killed before that leaves the folder behind, and the next run
  deletes it.
- **Play App Signing:** Google Play signs what it delivers with its own app signing key. Add that certificate's SHA-256
  (Play Console, *Test and release > App integrity*) to the flavor's `signers` once the app exists there.

The apps target API 36 and are linked against that platform (`platforms;android-36` must be installed); they run from
Android 8 (API 26).

### For a store (Google Play)

```
python android/build_apk.py --game <game> --bundle             # an Android App Bundle signed with the debug key (to try it)
python android/build_apk.py --game <game> --bundle --release   # signed with the upload key: what is uploaded
```

- `--bundle` makes `build-android/pakal-<game>-<version>.aab` with bundletool: download `bundletool-all-<version>.jar`
  from its GitHub releases to `build-android/bundletool.jar`, or point `BUNDLETOOL` (the environment or
  `local_paths.json`) at it. The bundle is checked with `bundletool validate`.
- `--release` signs with the upload key: `PAKAL_KEYSTORE` (the `.jks`) and `PAKAL_KEY_ALIAS`, in the environment or
  `local_paths.json`. Its passwords are asked with getpass at the terminal of the person running the build (only the
  person releasing signs), before the native build starts, and kept only in the run's memory. They are never read from a file,
  a config or the environment, and a run with no console is refused (on Windows the check is `GetConsoleMode`, since
  `isatty()` takes `NUL` for a console). apksigner and jarsigner get them through their own environment only. The store signs what it
  hands out with its own key (Play App Signing). Make the upload key once with `keytool -genkeypair -keystore
  upload.jks -alias upload -keyalg RSA -keysize 2048 -validity 10000` and keep a copy off the machine. A release with
  `--cheats` or `--tools` is refused. A release also has `PAKAL_RELEASE=1` in its default environment: what only the
  players' copies should do (a game's telemetry) can ask for it.
- The version: a game's `android.json` `"version_file"` names a JSON file with `"version": "MAJOR.MINOR.PATCH"`, or
  with `-beta.N`, `-rc.N` or `-alpha.N`. That is the version name, and the version code comes from it, each release
  above the one before and above its own betas: `1.0.0-beta.1` gives 1000021, `1.0.0-rc.2` 1000062, `1.0.0` 1000099.
  Without one, the version is `0.1+<build>.<commit>` and the code is the local build counter.
- An adaptive icon: `"icon_foreground"` (a square image with its subject inside the middle 66%) and `"icon_background"`
  (`#RRGGBB`) beside `"icon"`.
- With the bundle comes `pakal-<game>-<version>-native-debug-symbols.zip`: the native library before it was stripped, for the
  store to read its crashes (Play Console: the version's App bundle explorer, upload the debug symbols).
- To try a bundle as the store would install it: `--install-bundle SERIAL` (above).

## What changes compared to Windows

- `android/CMakeLists.txt` builds bx/bimg/bgfx (OpenGL ES 3.1, for compute shaders: a phone with only 3.0 does not run it; no Vulkan), Box2D 2.4.1 (the sources vcpkg extracted: its
  port tries to build glfw/X11 and fails on Android), the engine, the module and `android/main_android.cpp`.
- `PAKAL_NO_SFML` (Config.h): no SFML; the window, input and sound are provided by the platform. `PAKAL_STATIC_MODULE`
  (GameModuleLoader): the module is linked in, not loaded with `dlopen`.
- `WindowArgs.native_window` carries the `ANativeWindow*` to bgfx. On Android the default renderer is OpenGL ES.
- Shaders: the same `.sc` files, compiled with `--platform android -p 300_es` to `Assets/shaders/essl`.
- Touch input (`main_android.cpp`): left half = virtual stick, right half = aim and shoot. A touch that lands on a
  button of the game's interface (RmlUi: pause, the pause panel) or on a development panel (Dear ImGui) goes to it
  instead. How many fingers are down is told (`IInputManager::event_touch_count`): a gesture is the game's (the Zombies
  module shows its development panel with three fingers); the engine binds none. A game's own buttons are buttons of its
  interface that hold the button a moment (`InputMapper::tap`): the dungeon's abilities (in the bottom right corner,
  aimed at the nearest monster: it has no aim stick) and its bag,
  the brawler's HIT, HEAVY, JUMP, DODGE and GUARD. A game turns the sticks on while it is played
  (`IInputManager::set_touch_controls`); one that does not aim (the brawler) turns the right half's stick off
  (`set_touch_aim`), where its buttons are.
- A game can read the sticks itself instead (`IInputManager::set_game_touches`): every finger off the interface then
  goes to `event_touch_began/moved/ended` (screen pixels, `get_touch_area` is the screen's size) and the platform
  neither reads nor draws its sticks. TopDown games do this: `InputMapper::touch_sticks` turns the fingers into the
  move and aim sticks, and `TouchSticksPanel` draws them with an RmlUi document of the game's (its elements
  `move_base`, `move_thumb`, `aim_base`, `aim_thumb`; the body's classes `moving`, `aiming`, `firing`, and `hint_move` or
  `hint_aim` for the stick a game points out with `hint()`, a tutorial's lesson).
- The game reads its assets straight out of the APK through the resource manager (`AndroidAssetSource`). Nothing is
  copied to the app's storage. `PAKAL_EXTRACT=1` copies them as before (`assets.list` is the content version). Copies
  left by an older build are removed once.
- A sealed pack: a game's `android.json` `"pack"` (`{"file": "<name>.pak", "folders": ["<under Assets/>", ...], "key":
  "<a header of its folder with the key>"}`) seals those folders, as they were staged for the phone, into one pack at the APK's
  root (`tools/pakal_pack.py`, the format of a Windows package) and leaves them out of the APK loose: the art cannot be taken
  by unzipping it. The pack is stored uncompressed (`aapt2 -0 pak`; a bundle's `uncompressedGlob`), so `AndroidAssetSource`
  maps it in place (`ISource::map_resource`) and `ZipSource` reads it there: no copy to the app's storage, and memory holds
  only the pages read plus the file being opened. The game mounts it as on Windows, with
  `ResourceMgr.add_source<ZipSource>(root + "/<name>.pak", true, key)`. It deters copying by hand; it is not strong
  encryption (the key lives in the game).
- A store's build seals its art. `--release` refuses a game whose `android.json` names no `"pack"`. A game that means to
  ship its art loose says so, with `"pack": false, "pack_reason": "<why>"`. The engine's examples (zombies, brawler,
  dungeon) carry such a reason: their art is free. The refusal comes before the long build. After the pack is sealed, a
  release lists the folders of `Assets/` still loose in the package, with their sizes. Debug builds are as before.
  `pakal_pack.py` makes a large file's stream with numpy when it is installed (seconds for hundreds of MB; the same
  bytes).
- `build_apk.py` resamples the sounds to 22 kHz (`PAKAL_APK_FULL_AUDIO=1` keeps them as they are).

## Runtime

- `python android/build_apk.py` puts the levels in the APK; the game opens on its menu (the missions). `ZOMBIES_LEVEL` in
  `pakal_env.txt` (a full path) plays one level on its own instead.
- Debugging without a shell: whatever is put in `/sdcard/Android/data/<package>/files/pakal_env.txt` (`KEY=VALUE`
  per line) becomes an environment variable: `ZOMBIES_POST/SHADOWS/GI=off`, `ZOMBIES_AO`, `ZOMBIES_TILT`, `PAKAL_FPS_LOG=1`,
  `PAKAL_RENDER_HEIGHT`. With `PAKAL_EXTRACT=1`, assets are extracted again only if their hash changes (`assets.list`).
- Profiling and tuning from a computer: a build with `--tools` serves the engine's tools when `pakal_env.txt` has
  `PAKAL_MCP_PORT=<port>` (127.0.0.1 only, reached through `adb forward`); `tools/pakal_device.py` and the editor's
  Device window use them. A build without `--tools` (any release) has none. See docs/PROFILING.md.
- A game may send data to a server: `OSMgr.http_post(url, headers, body)` (blocking: from a thread of its own) goes through
  the activity (`PakalActivity.postBytes`, `HttpURLConnection`); the app's manifest must ask for INTERNET, and from Android
  9 only https is allowed. On Windows it is WinHTTP (`source/win32/HttpPostImpl.cpp`). The app going to the background and
  coming back are `OSMgr.event_app_paused` / `event_app_resumed` (a game writes there what it must not lose).
- The picture is drawn smaller than the screen (720 px high, 1080 at Ultra: `PAKAL_RENDER_HEIGHT`) and scaled up to the
  screen with FSR 1 (EASU rebuilds the edges, RCAS sharpens); the interface is drawn after it, at the screen's size, so its
  text stays sharp. The game's OPTIONS (menu and pause panel, saved in the profile) turn FSR off: the picture is then drawn
  at the screen's height up to the quality's cap (720, 1080 at Ultra) and stretched plainly. `PAKAL_FSR=0` leaves the scaling to the compositor (bilinear, the interface included); it wins over the game's saved FSR option for that launch. The screen's
  size is the window's (it extends under the navigation bar), not the content rect.
- A 2D game (`PAKAL_NO_3D=1`) is drawn at the screen's own size with no upscaler, and a game option asking for FSR is
  refused: sprites cost little to draw at any size, and FSR only blurs them. On a 2016 phone, FSR halved a 2D fight's frame
  rate (30 FPS against 60), and the full size cost nothing over half of it. `PAKAL_FSR=1` still turns it on for a launch.
- Touch: the sticks and the aim work in the picture's pixels, the interface in the screen's. A menu touch that moves less
  than 3.5% of the screen's height is a tap where it started, and one that lands that close to a button takes the nearest
  one; the development panels are bigger on Android and take touches a little around each control.

## Quality levels

Each level raises samples and resolution over the one before (`BgfxGraphicsSystem::Quality`). What each level draws is a
`BgfxGraphicsSystem::QualityPreset` (shadow map bounds, the map of only what moves under a lightmap, the shadow filter's
taps and spacing, whole or simplified casters, the post-process's share of samples, FXAA, bloom, the clouds' shadows): the
engine has the values below built in (`default_preset`), and a game can give its own with `set_quality_preset`, read
from its data when it starts (for example a `game/quality.json`, which can also say per level whether the screen-space
occlusion runs and how far models are drawn). Changing them needs no rebuild. The defaults:

| Level | Shadows | Post-processing (ambient occlusion, blur) | Render height |
|---|---|---|---|
| Minimum | 2048 map, edge over 3x3 texels, simplified casters | 30 % of the samples | 720 px |
| Medium | 2048 map, edge over 3x3 texels, simplified casters | 50 % | 720 px |
| High | what the level asks for, edge over 4x4 texels, whole casters | all | 720 px |
| Ultra | 4096 map at least, edge over 4x4 texels, whole casters | all | 1080 px |
| Extreme | as Ultra, and the clouds' shadows | as Ultra, and smoothed edges (FXAA) and bloom | 1080 px |

Extreme is never a device's default: the player picks it (the options) or `PAKAL_QUALITY=extreme`. What it adds:
- **Smoothed edges:** FXAA in the post pass, before the upscale (FSR rebuilds edges, it does not smooth them; MSAA would
  not leave the scene's depth for the post pass to read).
- **Bloom:** the frame's bright parts at a quarter of its size, blurred twice (a near glow and a wider one, views 9-13),
  added in the post pass. Only where the look asks for it: the `post` section's `bloom` (`strength`, `threshold`: the
  brightest channel 0..1 from which a pixel glows, `knee`: how softly).
- **The clouds' shadows:** soft patches of shade drifting over the town, in the lit shaders' sun visibility. The
  `lighting` section's `shadows.clouds` (`strength`: how much of the sun a cloud takes, `size`: meters, `wind`:
  meters a second, x and z).

The shadow's edge is filtered by the sampler's comparison (`u_shadowFilter`: taps on each side and their spacing): four
taps at every level: half a texel off the pixel at Minimum and Medium (an edge over 3x3 texels; an old phone's GPU spent
half its frame on nine plain ones), a texel off from High up (over 4x4). Sixteen looked the same and, with the screen full
of leaves, cost a tablet's GPU about an eighth of its frame rate.
`PAKAL_SHADOW_FILTER=wide|narrow|<taps>x<spacing>` measures another filter. Below High a character casts a simplified mesh (a third of its
triangles) and a tree's crown a clustered blob; at High and Ultra they cast all their triangles
(`MeshComponent3D_Bgfx::set_full_shadows`; static models take it when they load).
The wind in the foliage (`MeshComponent3D_Bgfx::set_wind(bend, speed)`, `pakal_wind.sh`) bends every alpha-tested material
(leaves, palms, grass) in the vertex shader, more the higher above its model's origin, each model on its own phase: vertex
work only, the same at every quality. The shadow map keeps the still shape.

At Minimum, a level with a lightmap keeps only what moves in the shadow map (1024): the fixed scene's shadow on the floor
is baked. `PAKAL_QUALITY=low|medium|high|ultra|extreme` sets it and `PAKAL_RENDER_HEIGHT` changes the height. Otherwise, on PC it is Ultra and on Android
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
`PAKAL_LOAD_LOG=1` logs each file a mounted source opens, with its time and size (`[Load] ms bytes path`: a sealed pack's
extracting it, the APK's reading it), to tell a load's hitch from a frame's.

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

### 2D games

- A 2D game passes the folders of its sprites, atlases and backgrounds to `finish_stage(stage, raw, sprite_dirs=[...])`
  (from its stager). Those textures are encoded at their size, without mips and at the best quality: they are drawn point
  sampled and 1:1 with the atlas rectangles, which a 1024 cap would shift. `SpriteComponent_Bgfx` reads through the
  resource manager (the APK) and takes `<file>.ktx` when there is one; a PNG too small to compress (64 px or less) stays PNG.
- The interface's images stay PNG: RmlUi decodes PNG/TGA itself and knows no `.ktx`. Any folder named `ui` is left alone.
- The mixer plays WAV only: `build_apk.py` converts the OGG files to 22 kHz WAV and rewrites the JSON documents that name
  them (needs `pip install soundfile`).
- A game reads its files with `ResourceMgr.read_text/read_bytes`, not `std::ifstream` (the APK is not a folder).
- On a Windows host, point `ANDROID_NDK` to a path **without spaces** (a junction does): CMake then calls `clang++` by its
  short name, which the driver does not recognise, and libc++ is not linked.

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
GPU), then the frame cap one step down (60, 45, 30 by default; a game sets its own, `set_governor_steps`, for example from
its `game/quality.json` "governor"), then a smaller picture. With room to spare (the frame's work fits well inside the next
step up) it steps back up one step at a time, and a step up that does not hold makes it wait longer before the next try.
  FSR is tried again once a launch: its cost is the GPU's, which a phone does not let it measure, so a retry that failed
  (three seconds at half the frames, a stutter) is not repeated.
- **What doesn't count:** the first eight seconds of a launch (the menu loading, the driver compiling its first programs),
  the first six seconds after a level loads (a phone's driver draws a new level slowly at first),
  and any time the game holds it (`hold_governor(true)`), e.g. while its camera pans over a whole town.
- **Turning it off:** `PAKAL_MAX_FPS` fixes the rate and turns it off; `PAKAL_GOVERNOR=0` turns it off too.

## Sound, lifecycle and controls

- **Sound**: `android/SoundManagerAAudio` mixes the WAV effects (16-bit) by hand into a single stereo AAudio stream; `AudioCues`
  uses it instead of SFML. The `.wav` files go in the APK.
- **Music**: `SoundManager::play_music` plays an OGG Vorbis file (`source/MusicMixer`, stb_vorbis from vcpkg's `stb`). On
  Android a thread of its own decodes it about 0.17 s ahead into a ring that the output callback reads, so the callback never
  decodes. The `.ogg` files go in the APK as they are (the 22 kHz resampling is for the WAV effects only).
- **One activity**: the activity is `singleTask`. Opened again from the launcher (or a store's Open button) while it
  runs, Android brings the running one to the front. A second instance in the same process started a second
  `android_main`, and the engine aborted as it was built again.
- **Background**: when going to the background (or losing the window) the engine pauses; on return, bgfx receives the new window
  (`BgfxGraphicsSystem::set_native_window`) and the engine continues where it was. Verified with HOME and reopening.
  The sound pauses with it (`Engine::on_pause` calls `SoundManager::set_paused`: the AAudio stream pauses, the music and the
  effects hold where they are) and goes on as the engine resumes and the window has the focus again (both: a Galaxy S24
  locked with its button gave the focus back behind the lock screen, and the music played). A stream that Android disconnects (its route changed:
  headphones, a call, a screen recorder) is closed and opened again, by its error callback's mark or when it does not
  start on resume. The stream asks for a session id of its own (Android 9+), which keeps AAudio off MMAP: an MMAP stream
  goes to the device past the system's mixer, and screen recorders cannot capture it. It still gets a fast track.
- **Touch controls**: two analog sticks. The left half moves and the right half aims;
  each stick appears where the thumb lands (120 px radius on a 1080-high screen, 15 % dead zone). The aim
  stick fires past 41 % (its disc turns red); without it, the hero faces where it walks. The screen is the engine's
  axis device 0 (`IAxisDevice`: axes 0-1 move, 2-3 aim), and a gamepad's two sticks are read there too while no thumb holds the screen's (the touch sticks hide while a pad plays; see SUBSYSTEMS.md, Gamepads). Restarting is in
  the pause panel (the button at the top right). The platform draws them with `BgfxGraphicsSystem::draw_screen_line`, unless the game reads the fingers itself (see
  `set_game_touches` above). `adb shell input swipe` drives them
  (in the coordinates of the phone's current resolution, not its panel's).
- **Movement** (same on PC): any tilt walks at full speed, starting accelerates (63 % in 0.1 s), on
  release it stops dead; the camera follows the hero with a short lag (SmoothDamp of 0.15 s) and the mouse does not turn it within
  0.5 m around it.
