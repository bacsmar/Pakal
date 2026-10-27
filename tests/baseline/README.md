# Behavior baseline

The reference a change is checked against. The game is deterministic: the same seed plays the same game, so a change
that keeps the game as it was gives the same traces as the ones stored here, line for line.

## How to check

```powershell
ctest --test-dir build-win64 --output-on-failure
powershell -File tools\bot_check.ps1
```

`bot_check.ps1` plays every `<level>_seed<n>.trace` of this folder with the bot (`tools/bot_run.ps1`) and prints `SAME` or
`DIFFERS` per case. A trace is the game's state line every 2 s of game time (time, kills, enemies alive, health, money,
ammunition, the player's position, the spawn orders) up to the victory, the defeat or the failure.

A change that alters the game on purpose (new rules, a balance change, another random stream) regenerates the traces
in the same commit: run `bot_run.ps1` for each case and copy its `.trace` here, and say why in the commit.

The traces hold for this build on this kind of machine: they depend on the window's shape (the zombies far off screen
walk faster), and floating point results may differ with another compiler or processor.

Captures (not kept here): `bot_run.ps1 ... -ExtraEnv @{ ZOMBIES_AUTOPLAY = "idle"; ZOMBIES_SCREENSHOT = "<file>.tga"; ZOMBIES_SCREENSHOT_AT = "4" }`
for the start, or with the bot playing and `ZOMBIES_SCREENSHOT_AT = "45"` to see the zombies.
