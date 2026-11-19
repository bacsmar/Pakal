# Basic Example

This is the minimal executable example for Pakal Engine startup and shutdown.

## Building

From the project root, as in [docs/QUICKSTART.md](../../docs/QUICKSTART.md):

```bash
cmake --preset linux-release        # or win64 on Windows
cmake --build --preset linux-release # or win64-release
```

## Running

```bash
cd build-linux/bin   # build-win64\bin on Windows
./PakalBasicExample
```

## What it does

- Initializes the Pakal Engine with default settings.
- Runs the main loop briefly.
- Emits the app-finished event.
- Cleans up and terminates.

For camera, sprite transform, and audio diagnostics, use `PakalFeatureLab` from `examples/engine_feature_lab`.
