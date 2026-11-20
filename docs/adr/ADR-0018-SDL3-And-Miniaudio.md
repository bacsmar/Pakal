# ADR-0018: SDL3 and miniaudio Replace SFML

Status: Accepted (2026-11-16). Step 0 is done; the rest is in progress.

## Context
SFML 2.6 (vcpkg, static) is used only on desktop, for the window, the keyboard, gamepads and audio. It lives in about 8
files under `source/sfml`. Android does not use it (`PAKAL_NO_SFML`): it has a native window and
`android/SoundManagerAAudio`. So there are already two window/input backends and two audio backends.

SFML has no Emscripten port and weak mobile support, and its version 3 changes the event API, so moving to it would
rewrite the same files and gain nothing. A web build and iOS are both likely targets. bgfx supports WebGL2 and iOS, and
needs only a native window handle. Box2D, Lua and the games are portable, and ADR-0017 keeps the games away from the
backends.

`SoundManagerSFML.h` exposed `sf::` types, so SFML's include directories had to be PUBLIC on the Pakal target. That broke
ADR-0017 whatever backend came next.

## Decision
- **Window, input and gamepads: SDL3.** It is one backend for Windows, Linux, macOS, iOS and Emscripten, and it comes
  with the community gamepad mapping database. It goes in `source/sdl/` and implements the same abstractions that
  `source/sfml/` implements today: `InputManager`, the window creator, focus, keyboard and gamepads.
- **Audio: miniaudio.** It is a single public-domain file. It decodes OGG, WAV and MP3, and it has WASAPI, AAudio,
  CoreAudio and WebAudio backends. It implements `SoundManager`, `play_effect`, the music stream and the sound
  components. SDL3's audio is not used because it does not decode OGG.
- **Android keeps its native activity and window.** SDL on Android needs its Java activity, which would change the APK
  packaging and the pause/resume path. miniaudio may replace `SoundManagerAAudio` only if it measures as well or better
  on latency and pause/resume, on a recent phone and an old one.
- **No `sf::` or `SDL_`/`ma_` types in a public header.** Every backend type stays in `.cpp` files or private headers,
  and the backend's include directories are PRIVATE.
- The web build is a separate decision. It needs a spike first, and its findings go in its own ADR.

## Plan
0. Done: close the SFML leak. No `sf::` type in `SoundManagerSFML.h`, and the SFML includes are PRIVATE.
1. Add `source/sdl/` and a miniaudio `SoundManager`, chosen in CMake with `PAKAL_DESKTOP_BACKEND=SDL3|SFML`. SDL3 is the
   default on Windows, and SFML stays as a fallback for one release.
2. Check parity on Windows: effects with volume and pitch, the streamed music (`MusicPlayer`) and `AudioCues`, by ear;
   the gamepads (two pads, and the touch controls that hide when a pad plays), the window focus, and all of ctest.
   Then remove SFML.
3. Optional: test miniaudio against AAudio on Android, and keep the better one.
4. A WebGL spike, in its own ADR. It must cover `emscripten_set_main_loop`, threads (COOP/COEP or none), KTX2/Basis
   instead of ASTC, loading by parts, fetch, IDBFS for saves, and WebSocket instead of ENet.
5. iOS on the same backend, later.

## Consequences
- One desktop backend that also runs on iOS and the web, instead of a third backend for each platform.
- Better gamepad mapping on desktop.
- One audio library could later serve every platform.
- Risks: audio parity, and pause/resume on Android. That is why Android changes only after measuring.
- Two new vcpkg dependencies (`sdl3`, `miniaudio`), and SFML is dropped.

## Related Documents
- ADR-0017 Games Never See the Backends, ADR-0019 Library Upgrades
