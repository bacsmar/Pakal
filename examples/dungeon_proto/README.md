# Dungeon prototype

A third game on the TopDown module (`game_genres/topdown`), a small Diablo-like that tries the RPG kit
(`game_genres/topdown/kits/rpg`). A dungeon of nine rooms joined by corridors is made from a seed. The hero (Sam, with an
axe) fights its way to the Butcher in the deepest room. Monsters give experience and drop gold and items, chests open
when a hero comes near and spill theirs, and the hero levels up and wears what it finds. The fallen leave a pool of
blood under them.

## Run

```
build-win64\bin\PakalPlayer.exe DungeonModule.dll
```

It opens on a menu: play alone, host a game, or join one found on this network (see below). On Android it is
`pakal-dungeon.apk` (docs/ANDROID.md): the left half of the screen walks; the abilities are buttons in the bottom right
corner (STRIKE the biggest, whirlwind, fireball and heal around it) and aim by themselves at the nearest monster in
sight (`DungeonGame::auto_aim`, what any unaimed ability does); BAG and the pause at the top right.

It needs:
- the Zombies example's models (`Assets/zombies/downtown/meshes`) for the characters, the axe and the loot;
- the Medieval Village pack in `examples/zombies_module/assets/source/medieval-village` for the dungeon's floor, walls
  and props. CMake copies the pieces it uses to `Assets/dungeon/meshes`.

Both are installed with the free packs (see docs/ASSET_INDEX.md).

| Key | Does |
|---|---|
| WASD | Walk |
| Mouse | Aim; click to strike (the first ability) |
| Q or 2 | Whirlwind: hits everything around (mana) |
| E or 3 | Fireball: bursts on the first monster or wall (mana) |
| F or 4 | Heal: 40% of the health (mana) |
| I | Inventory: click an item to wear it, a worn one to take it off (online too: each player has its own) |
| P or Escape | Pause (alone the dungeon waits; online it goes on) |
| R | A new dungeon (the next seed after a win, the same one after a defeat) |

| Variable | Effect |
|---|---|
| `DUNGEON_PLAY=1` | Straight to the dungeon, alone, without the menu (so do the bot, a capture and `DUNGEON_CONNECT`) |
| `DUNGEON_AUTOPLAY=1` | A bot plays (the keys do nothing) |
| `DUNGEON_SEED=<n>` | The dungeon (layout, monsters, loot); 2026 by default |
| `DUNGEON_TIMESCALE=<x>` | Game seconds per real second |
| `DUNGEON_SSAO=on\|off` | The screen's ambient occlusion (SSAO); by default only at ultra quality: the occlusion is baked |
| `DUNGEON_BAKED_AO=off` | Without the baked occlusion (the light grid) |
| `DUNGEON_MERGE=off` | Every floor tile and prop drawn on its own (they are merged by model, 8 x 8 cells a block) |
| `DUNGEON_CAM_SCALE=<x>` | The camera this much farther (below 1: closer), to look at the models |
| `DUNGEON_SCREENSHOT=<file>.tga`, `DUNGEON_SCREENSHOT_AT=<s>` | A capture after that many seconds |
| `DUNGEON_CONNECT=<host>[:<port>]`, `DUNGEON_NAME=<name>` | Plays on a `DungeonServer` (port 27960 by default) instead of alone; the name is the device's otherwise |

## Together, over the network

The menu lists the games on this network: a `DungeonServer.exe` (built next to the player, no window), or a player who
chose HOST A GAME. A player who joins gets a hero in the first room, and everyone fights the same monsters. See
docs/NETWORK.md.

## Files

- `data/dungeon.json` holds the rules:
  - the dungeon: the room grid, room sizes, corridors, monsters per room and how many more the deeper, the boss;
  - the hero's growth (`progression`: experience per level, attributes and what each point gives);
  - the creatures: health, speed, body, abilities (`AbilityRules`: arc, area, projectile, heal), how a monster fights,
    what it is worth, and how it looks;
  - the items (a slot and stat bonuses) and the loot tables (one for the chests: `chestLoot`, rolled `chestRolls` times;
    `chestChance` of a chest in a room, the first room always has one);
  - the buttons, and the look: camera, light, grading, and the dungeon's pieces.
- `DungeonGame.h/.cpp` has no graphics; `tests/DungeonTests.cpp` plays it without a window. It holds:
  - the rules;
  - the layout (`DungeonLayout`: rooms on a grid, a random spanning tree of corridors and a few loops, the floor in
    cells; a wall on every open side, then the walkable grid);
  - the simulation (`DungeonGame`, a `TopDownGame`: experience for the hero, loot dropped as `LootComponent` piles, the
    chests (`ChestComponent`), the boss, and the inventory's commands: `gear_command`, `apply_gear_command`);
  - the monsters' brain (`MonsterBrainComponent`: asleep until the hero is near and in sight, then it follows the
    walkable grid; ranged ones keep their distance);
  - the bot.
- `DungeonLevelState.h/.cpp` runs the level on TopDown's `LevelRuntime`:
  - the dungeon's pieces, one floor tile per cell and one wall per open side; walls between the camera and the hero fade
    (`OccluderFade`). The floor's tiles and the props are merged by model, a block of 8 x 8 cells at a time
    (`MeshComponent3D_Bgfx::merge`: one draw per block, still culled block by block); the walls stay apart, each fades
    on its own;
  - the ambient occlusion, baked when the dungeon is built (`bake_occlusion`): the light grid (the engine's light probes,
    one voxel each half meter, four layers up to the walls' top) holds the ambient light the walls let reach each spot,
    so the floor darkens by the walls and in the corners with one texture read a pixel. The screen's occlusion (SSAO)
    runs only at ultra;
  - the characters, and the loot piles' models;
  - the missiles (particles), with the bursts and blows drawn by `EffectsSystem`; blood where a character is hurt (from
    its hurt count, so a client shows it too), a pool of the pack's blood under each body, the chests (dark and a spray
    of sparks once open);
  - the HUD (`data/ui/hud.rml` through `UiPanel`): health and mana flasks, experience, the abilities with their cooldowns
    and whether they can be paid, messages, the inventory, the pause and the result. The inventory is drawn from the
    hero's own data (`OwnState`), the same alone and online.
- `DungeonMenuState.h/.cpp`: the first screen (`data/ui/menu.rml`): alone, host, or the games found on the network.
- `DungeonModule.cpp`: the module PakalPlayer loads.
- `DungeonNet.h/.cpp`: network play: the seed a client builds the dungeon from, a hero's own data, and the server's
  loop (`DungeonServer`, with bots for heroes nobody plays), and `DungeonHost`, that server on a thread of a player's game.
  `DungeonServerMain.cpp`: the headless server.
  `tests/NetTests.cpp` plays it over UDP on this machine.

## What it needed from TopDown

Everything in the RPG kit was written for it. Besides the kit, it needed:
- `CharacterBodyComponent::set_enabled`, so a body on the floor does not block the way;
- `LevelRuntime::add_view`, which now attaches a look's `held` mesh;
- `UiPanel::property`, which sets one property of an element's style (the flasks fill upward).
