# Brawler prototype

A second game on the TopDown module (`game_genres/topdown`), to try the module with something that is not Zombies: one
arena, hand to hand, a hero (Matt) against a thug and a brute. It uses the Brawler kit
(`game_genres/topdown/kits/brawler/FighterComponent`), heights (`HeightComponent`), actions with phases (`ActionTimeline`) and
clip marks (`MeshComponent3D_Bgfx::add_clip_mark`). Nothing else in `game_genres/topdown` was changed for it.

## Run

```
build-win64\bin\PakalPlayer.exe BrawlerModule.dll
```

It needs the Zombies example's models (`Assets/zombies/downtown/meshes`, installed with the free packs; see
docs/ASSET_INDEX.md). Keys: WASD walk, J light (pressed again: jab, jab, cross), K heavy, I guard (held: a fifth of a hit from the front, no
stun), L dodge (a roll that nothing hits), Space jump (J or K in the air: a kick), R fight again. The hero's punches and roll are clips of the animation library retargeted onto the kit's characters
(`PLAYER_UAL_CLIPS` in tools/zombies_convert/build_downtown.py).

| Variable | Effect |
|---|---|
| `BRAWLER_AUTOPLAY=1` | A bot fights (the keys do nothing) |
| `BRAWLER_SEED=<n>` | Random seed (the enemies' choices) |
| `BRAWLER_CLAY=<0..1>` | How much plasticine the fighters are (the arena's `characters.clay`, 1 by default) |
| `BRAWLER_SKETCH=<strength>` | The sketch look of the Zombies menu's LOOK option (0.0 to 1.0) |
| `BRAWLER_CAM_SCALE=<x>` | The camera this much farther (below 1: closer), on the hero, to look at the models |
| `BRAWLER_SCREENSHOT=<file>.tga`, `BRAWLER_SCREENSHOT_AT=<s>` | A capture after that many seconds |

## Files

- `data/brawler.json`: the arena, its look (the Zombies levels' `lighting` and `post` sections: sun and shadows,
  ambient light, plastic gloss, grading, tilt-shift, occlusion), the fighters (health, speed, body, moves, how the enemies fight, how each one looks:
  its model, clips by what it does, and the marks of its clips) and the buttons (each tries its moves in order).
- `BrawlerGame.h/.cpp`: the rules, the simulation (`BrawlerGame`, a `TopDownGame`), the enemies' brain and the bot.
  No graphics: `tests/BrawlerTests.cpp` plays it without a window.
- `BrawlerLevelState.h/.cpp`: on TopDown's `LevelRuntime` (camera, input, models, look, fixed steps): the camera from the
  front (it follows the hero and stops before the arena's ends), the buttons, and the HUD (`data/ui/hud.rml`, an RmlUi
  document through `UiPanel`: everyone's health, the hits, the result with FIGHT AGAIN).
- `BrawlerModule.cpp`: the module PakalPlayer loads.

A move's windup should take about the same share of the move as its clip takes to its "impact" mark: the view
stretches the clip over the move, so the fist lands when the hitbox comes out.
