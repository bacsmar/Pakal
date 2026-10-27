# Pakal core

The core of the Pakal engine, alone: the engine, its systems' interfaces, entities and components, tasks and events,
resources, persistence, logging and telemetry. No graphics, physics, sound or window backend: the dummy systems
(`DummyGraphicsSystem`, `DummyPhysicsSystem`, `DummyUIManager`, `DummyWindowManager`, `DummyInputManager`) take their
place, so the engine starts, runs its systems and ends without any of them.

The full engine (bgfx rendering, Box2D physics, SFML, Android, the TopDown genre module and the examples) is on the
`master` branch, where this same core is built with every build (`cmake/PakalCore.cmake`).

## Build

```bash
git clone -b core https://github.com/bacsmar/Pakal.git
cd Pakal
git submodule update --init
```

Windows, from a Developer shell (vcpkg at `VCPKG_ROOT`):

```bat
cmake --preset win64
cmake --build --preset win64-release
ctest --test-dir build-win64 --output-on-failure
```

Linux: `cmake --preset linux`, `cmake --build --preset linux-release`, `ctest --test-dir build-linux`.

The only dependency is nlohmann-json (header only), for the telemetry's records.

## Layout

- `source/`: the core. `Engine` (`Engine.h`) runs the systems an `Engine::Settings` allocates; `PAKAL_CORE_ONLY`
  (`Config.h`, set by the `PakalCore` target) makes those the dummy ones.
- `source/persist`: the persistence library (a submodule): archives in XML, JSON and text.
- `tests/CoreTests.cpp`: starts the engine with the dummy systems, runs it a moment and ends it.

## License

MIT, see [LICENSE](LICENSE).
