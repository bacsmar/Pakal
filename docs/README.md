# Pakal documentation

This folder contains the architecture documentation and usage examples for the Pakal engine.

Included files:
- `QUICKSTART.md` — From an empty folder to the engine running (Windows, Linux, Android).
- `ARCHITECTURE.md` — Overview and execution flow.
- `SUBSYSTEMS.md` — Documentation per subsystem with paths to the relevant files.
- `USAGE.md` — Minimal guide to build and run, `main.cpp` example.
- `EXAMPLES.md` — Practical examples with snippets.
- `QA_HOOKS.md` — Driving a running game from a program (the MCP QA hooks: input, state, log, time scale, quit) and the scripted smoke stories (`tools/pakal_device.py story`).
- `UI_RELOAD.md` — Reloading `.rml`, `.rcss`, texts and data `.json` while the game runs (dev and `--tools` builds), and the C-DAT scan (`tools/cdat_scan.py`).
- `SPLASH.md` — The logo at start (SplashState): fixed with a loading screen, or covering the load and skippable.
- `RELEASE_CHECKLIST.md` — What a finished game should hold before release, item by item (C-ID, C-BLD, C-OPT, C-DAT...): the codes the engine's code and tools name.
- `RELEASE_CHECK.md` — `tools/release_check.py`: checks a game's finished APK or bundle before release, and how a game adds its own checks.
- `BUILD_IDENTITY.md` — Which copy is running: build.json, the channel, the build label the engine draws, and hiding it in production.
- `SETTINGS.md` — The options every game has (GameSettings: music, sounds, language, telemetry, quality...) and the about data.
- `PARTICLES.md` — Particle effects as JSON files (ParticleEffects, IParticleRenderer).
- `ATLAS.md` — Sprite atlases with polygon meshes (`tools/pakal_atlas`, SpriteAtlas).
- `adr/` — Architecture decisions: games never see the backends (ADR-0017), SDL3 and miniaudio (ADR-0018), library upgrades (ADR-0019).
