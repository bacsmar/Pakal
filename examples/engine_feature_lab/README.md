# Engine Feature Lab

`PakalFeatureLab` is a focused diagnostic executable for engine subsystems that are too noisy to validate inside a full game.

## Build

```bash
./docker-build.sh build all debug
```

## Run

```bash
cd docker-build/bin
./PakalFeatureLab
```

## What It Tests

- `CameraComponent2D` with a 32x18 world-space orthographic view.
- Solid-color `SpriteComponent2D` quads for background, axes, layer order, scale, and rotation checks.
- Per-frame sprite rotation and scale animation.
- Slow camera movement to expose projection or positioning mistakes.
- In-memory WAV generation for `SFXComponent` and `MusicComponent`, without external audio assets.

If this lab renders at a sane size but ContraGame looks tiny, ContraGame has a scene-camera world-unit mismatch. If this lab is tiny too, the bug is in the shared sprite/camera pipeline.