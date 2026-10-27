# ContraGame Assets

This folder holds all the assets (resources) of the ContraGame example. The sprites are not published in the public
repository yet (the example is unfinished).

## Folder layout

```
Assets/
├── sprites/              # Sprite images (characters, enemies, platforms, etc.)
├── audio/                # Sound files (music, sound effects)
├── animations/           # Animation definitions (JSON/XML)
└── maps/                 # Map and level data (Tiled, JSON, etc.)
```

## How it works

The ContraGame build copies this `Assets/` folder next to the executable automatically:

```
docker-build/bin/ContraGame      # Executable
docker-build/bin/Assets/         # Copy of this folder
```

## Supported formats

### Sprites (sprites/)
- **PNG** - Recommended for sprites with transparency
- **BMP** - Simple format without transparency
- **TGA** - For higher quality

### Audio (audio/)
- **WAV** - Sound effects
- **OGG** - Compressed music
- **MP3** - Alternative music format

## Recommended layout example

```
Assets/
├── sprites/
│   ├── player/
│   │   ├── idle.png
│   │   ├── jump.png
│   │   ├── run.png
│   │   └── death.png
│   ├── enemies/
│   │   ├── soldier.png
│   │   └── boss.png
│   └── tiles/
│       ├── platform.png
│       └── background.png
├── audio/
│   ├── music/
│   │   └── level1.ogg
│   └── sfx/
│       ├── jump.wav
│       └── shoot.wav
└── maps/
    ├── level1.json
    └── level2.json
```

## Loading assets in code

Use the engine's `ResourceManager` to load assets:

```cpp
// Load a texture
auto resource = ResourceManager::instance().get_resource("Assets/sprites/player/idle.png");

// Assets are looked up relative to the executable's folder
```

## Notes

1. **Relative paths**: assets are loaded from paths relative to the executable
2. **Syncing**: after adding assets, rebuild the project so they are copied
3. **Names**: use file names without spaces, lower case if possible
