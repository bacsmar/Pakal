# Third-party assets

## Zombie Apocalypse Kit (March 2024), Quaternius

Location: `examples/zombies_module/ZombieApocalypseKit-March-2024`.

- Author: Quaternius (https://www.patreon.com/quaternius). The kit's own `License.txt` states CC0 1.0 Universal (public domain dedication): free to use, modify and redistribute, including commercially, without asking permission or giving credit. Credit is given here anyway.
- Used for: the player (`Characters_Sam_SingleWeapon.gltf`) and the zombies (`Zombie_Basic.gltf`), in `ZombieConverter --kit`. Their glTF files are self-contained (buffers embedded) and carry their animation clips, which the engine plays by name.

Conversion: `ZombiesConvert.exe ... --kit <kit folder> --role enemy=Zombie_Basic --role player=Characters_Sam_SingleWeapon`.

## Additional CC0 city and animation packs

The supplied Downtown City and Universal Animation Library packs are organized
under `examples/zombies_module/assets/source/`. Their included license files state
CC0 1.0 Universal. See `examples/zombies_module/assets/README.md` for the inventory
and intended uses. Raw packages are ignored by Git; licenses and alternate exports
remain available locally.

Downtown City's glTF exports use external `.bin` buffers and image files; unlike
the character kit, copying only a `.gltf` is insufficient.
