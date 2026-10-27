# ContraGame Architecture

## Overview

ContraGame is compiled as a **dynamic module (libContraGameModule.so)** for the Pakal Engine, NOT as a standalone executable.

## Build Artifacts

```
Pakal Core (source/)
├── CMakeLists.txt
├── source/
│   ├── PakalPlayerMain.cpp          ← SINGLE runtime host (generic for any .so)
│   └── (rest of core...)
└── docker-build/bin/
    ├── PakalPlayer                   ← Executable (loads any .so plugin)
    └── libContraGameModule.so         ← ContraGame plugin

examples/ContraGame/
├── CMakeLists.txt                    ← Produces ONLY .so (no executable)
├── ContraGameModule.cpp              ← Entry point: pakal_get_game_module_exports()
├── main.cpp                          ← Reference only (NOT compiled as executable)
└── (game code...)
```

## Execution Flow

```
$ PakalPlayer libContraGameModule.so
  1. PakalPlayer loads Pakal core
  2. GameModuleLoader::load("libContraGameModule.so") via dlopen()
  3. Resolves symbol: pakal_get_game_module_exports()
  4. Initializes game factories and runs game
```

## Why main.cpp exists but is NOT compiled

The `main.cpp` file is kept as a **reference implementation** showing how a standalone 
ContraGame executable could work, but:

- **It does NOT compile as part of libContraGameModule.so** (excluded in CMakeLists.txt line 32)
- **It does NOT compile as part of Pakal core** (stays in examples/)
- **It has no purpose** in the current architecture

### To use ContraGame:

```bash
# Correct way (via PakalPlayer)
./bin/PakalPlayer ./lib/libContraGameModule.so

# Not used:
# ./bin/ContraGameModule (doesn't exist)
```

## Design Decision

This separation of concerns follows plugin architecture best practices:

- **Pakal Core** provides: Engine, systems, base classes
- **PakalPlayer** provides: Generic runtime host to load any game module
- **ContraGame** provides: Game-specific logic as a loadable plugin

This allows:
- Multiple games to share the same PakalPlayer executable
- Game code to be isolated in separate modules
- Easy module replacement without rebuilding core
