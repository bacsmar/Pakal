# Engine Feature Lab

`PakalFeatureLab` is a focused diagnostic executable for engine subsystems that are too noisy to validate inside a full game.

## Build

From the project root, as in [docs/QUICKSTART.md](../../docs/QUICKSTART.md):

```bash
cmake --preset linux-release        # or win64 on Windows
cmake --build --preset linux-release # or win64-release
```

## Run

```bash
cd build-linux/bin   # build-win64\bin on Windows
./PakalFeatureLab
```

## What It Tests

- `CameraComponent2D` with a 32x18 world-space orthographic view.
- Solid-color `SpriteComponent2D` quads for background, axes, layer order, scale, and rotation checks.
- Per-frame sprite rotation and scale animation.
- Slow camera movement to expose projection or positioning mistakes.
- In-memory WAV generation for `SFXComponent` and `MusicComponent`, without external audio assets.

If this lab renders at a sane size but ContraGame looks tiny, ContraGame has a scene-camera world-unit mismatch. If this lab is tiny too, the bug is in the shared sprite/camera pipeline.