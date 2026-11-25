# Modular characters

A character made of parts: the player picks a body, a head, hair and gear, and the game draws the result as one
skinned model in one draw call. The parts are skinned to the same skeleton, matched by bone names. Assembly happens
once, when the character is made or changed, so nothing of the assembly costs anything per frame.

## The recipe

`Pakal::CharacterRecipe` (`source/CharacterRecipe.h`) is plain data with no backend. A game builds it in its
customization menu, saves it with the profile and sends it over the network instead of a mesh.

```json
{
  "skeleton": "characters/base_rig.glb",
  "parts": [
    { "slot": "body",  "file": "characters/body_female.glb" },
    { "slot": "head",  "file": "characters/heads.glb", "mesh": "Head_03" },
    { "slot": "hair",  "file": "characters/hair_long.glb" },
    { "slot": "torso", "file": "characters/jacket.glb" }
  ],
  "shapes": { "masculineFeminine": 1.0, "defaultHeavy": 0.3 },
  "foldBones": ["hair_dyn_"]
}
```

| Field | Meaning |
|---|---|
| `skeleton` | The file whose bones (hierarchy and rest pose) and animation clips the character takes. Empty means the first part's. |
| `parts` | One per slot. `file` is a model skinned to the skeleton. `mesh` takes only the meshes of that name, for a file that holds every variant. A part with no bones (a hat, a rigid shield) follows the bone named like the node it hangs from in its own file. |
| `shapes` | Blend shapes baked into the vertices at assembly, by name (body build, a face's expression). A part without that shape is left as it is. The assembled model has no blend shapes left, so they cost nothing at run time. |
| `foldBones` | Bone name prefixes whose weights go to the nearest bone above them. Use them for a rig over the skinning limit of 64 bones, for example dynamic hair bones, or fingers that a top-down camera never shows. |

Relative files are taken from the recipe's folder (`from_json(text, baseDir, error)`). `to_json()` writes it back.

## Assembling

`MeshComponent3D_Bgfx::load_character(recipe)` assembles the parts (`CharacterAssembly`) into one Assimp scene and
loads it the same way a model file is loaded: textures, materials, clips, the pose workers and attachments all work as
for any skinned model. The assembly does these things:

- **Bones:** each part's bones are matched to the skeleton's by name. Bones that no vertex uses are left out, and the
  folded bones are merged into their parent. More than 64 bones left is refused, with the count.
- **Materials and textures:** materials that are the same (texture, color, blend mode) are kept once, and so are
  embedded textures with the same bytes. Parts painted from one atlas, as a pack's are, draw in one call. The log line
  says how many materials are drawn.
- **Shapes:** baked as `vertex + weight x (shape vertex - vertex)`, positions and normals.
- **Clips:** they come from the skeleton file, and any clips borrowed onto the model afterwards work as before.

The log reports what was assembled, for example
`character assembled: 3 parts, 3 meshes, 19 bones, 1 materials drawn, 1 textures, 27 clips`.

## In a TopDown look

A character's look (`CharacterViewComponent::Look`) takes a recipe where it takes a model file:

```json
{ "recipe": "characters/onlooker_a.json", "recipes": [ "characters/onlooker_b.json", { "parts": [ ... ] } ],
  "height": 1.75, "clips": { "idle": ["Idle"], "move": ["Walk"] } }
```

- `recipe` is one recipe: inline, or a recipe file's path, whose parts' relative files are taken from the file's
  folder. `recipes` adds more, one variant each, after `recipe`, as `variants` does for model files. When a look has
  recipes, they stand for `model` and `variants`. A recipe that does not read is logged and left out.
- The look's other fields (`clips`, `height`, `actions`, `weapons`, `held`, `marks`) work as they do for a model. The
  clips are the recipe skeleton's.
- `LevelRuntime::add_view(actor, look, defaults, onMark, clay, dents, variant)` draws the recipe `variant` (modulo how
  many there are). `LevelRuntime::character_model(look, variant)` gives the model alone, for a game's own pool.
- Each recipe is assembled once per level (`LevelRuntime::character`): its copies share it, as a model file's copies
  share its prototype, until `end()`. A crowd of many people drawn from a few recipes costs a few assemblies, and the
  package carries only the parts.

## Trying it

- `tools/modular/split_character.py` (Blender) cuts a skinned character into head, torso and legs. It gives the
  torso and the legs a `defaultHeavy` shape. The free kits' characters share one skeleton and one atlas, so parts of
  two of them make one character:

  ```
  blender -b -P tools/modular/split_character.py -- Characters_Lis_SingleWeapon_UAL.glb out Lis
  blender -b -P tools/modular/split_character.py -- Characters_Matt_SingleWeapon_UAL.glb out Matt
  ```
- `PakalMeshViewer <recipe>.json` shows a recipe: a `.json` model is read as one. With Lis's torso and legs, Matt's
  head and Lis's skeleton, it plays Lis's clips in one draw.
- `CharacterAssemblyTests` writes two tiny glTF parts and checks the merge, the shared material, the baked shape, the
  folded bone and the limits.

## Not yet

- **Colors:** recoloring by slot (skin, hair, cloth) by writing a palette texture at assembly.
- **Packs that ship FBX only** (some commercial modular packs): an offline converter to glTF that keeps the blend shapes and bone
  names. Their files are licensed to their buyer and never enter the engine's repository.
- **Recipes over the network.**
- **Loading off the graphics thread** (`decode` for a recipe). Each recipe reads its parts' files again, and recipes
  sharing a part each upload its textures: a cache of the parts read, and of their textures, would share them.
- **Random recipes:** a look built from slots and choices ("any of these heads") instead of a list of whole recipes.
