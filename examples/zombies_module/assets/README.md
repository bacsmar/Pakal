# Zombies source assets

The local packs are kept in `source/`, outside the Git history.
No originals or variants were deleted. The license files are kept
inside each pack.

| Quaternius pack | Folder | Intended use |
|---|---|---|
| Downtown City, Standard | `source/downtown-city` | Buildings, streets, sidewalks, roofs and urban props |
| Universal Animation Library, Standard | `source/universal-animation-library` | Character animations; includes variants with and without root motion |
| Universal Animation Library 2, Standard | `source/universal-animation-library-2` | Additional animations and female mannequin |

The included licenses state **CC0 1.0 Universal**. Downtown City contains
153 glTF models, their external `.bin` buffers, textures and FBX exports.
Keep the glTF files next to their buffers and the relative paths of their textures.
The textures include materials that need additional shaders (interiors,
wear and maps other than base color); not all of them are compatible with the
engine's current shader.

The animation libraries need a skeleton compatibility check or
retargeting before they replace the clips of the current characters. They are not
connected to the runtime or to the Survival conversion yet.

The Zombie Apocalypse kit already in use stays in
`../ZombieApocalypseKit-March-2024` to keep the existing conversion
commands working. The reorganization does not change the layout or the collisions of the level.
