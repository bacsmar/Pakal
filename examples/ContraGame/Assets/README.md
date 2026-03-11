# ContraGame Assets

Este directorio contiene todos los assets (recursos) del juego ContraGame.

## Estructura de Directorios

```
Assets/
├── sprites/              # Imágenes de sprites (personajes, enemigos, plataformas, etc.)
├── audio/                # Archivos de sonido (música, efectos de sonido)
├── animations/           # Definiciones de animaciones (JSON/XML)
└── maps/                 # Datos de mapas y niveles (Tiled, JSON, etc.)
```

## Cómo Funciona

El sistema de compilación de ContraGame copia automáticamente este directorio `Assets/` al directorio de salida del ejecutable:

```
docker-build/bin/ContraGame      # Ejecutable
docker-build/bin/Assets/         # Copia de este directorio
```

## Formatos Soportados

### Sprites (sprites/)
- **PNG** - Recomendado para sprites con transparencia
- **BMP** - Formato simple sin transparencia
- **TGA** - Para mayor calidad

### Audio (audio/)
- **WAV** - Efectos de sonido
- **OGG** - Música comprimida
- **MP3** - Música alternativa

## Ejemplo de Estructura Recomendada

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

## Cómo Cargar Assets en el Código

Usa el `ResourceManager` del engine para cargar assets:

```cpp
// Cargar textura
auto resource = ResourceManager::instance().get_resource("Assets/sprites/player/idle.png");

// Los assets se buscan relativo al directorio del ejecutable
```

## Notas Importantes

1. **Ruta relativa**: Los assets se cargan desde rutas relativas al ejecutable
2. **Sincronización**: Después de agregar assets, debes reconstruir el proyecto para que se copien
3. **Nombres**: Usa nombres de archivo sin espacios, en minúsculas si es posible
