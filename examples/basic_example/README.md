# Basic Example

This is the minimal executable example for Pakal Engine startup and shutdown.

## Building

From the project root:

```bash
./docker-build.sh build all debug
```

## Running

```bash
cd docker-build/bin
./PakalBasicExample
```

## What it does

- Initializes the Pakal Engine with default settings.
- Runs the main loop briefly.
- Emits the app-finished event.
- Cleans up and terminates.

For camera, sprite transform, and audio diagnostics, use `PakalFeatureLab` from `examples/engine_feature_lab`.
