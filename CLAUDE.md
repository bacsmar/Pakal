# Pakal

C++20 game engine (bgfx, Box2D, Lua) with a Zombies example module. See README.md and docs/.

- Building levels from the free Quaternius packs: tools/levelbuilder/CLAUDE.md (the level builder's workflow) and
  docs/LEVEL_BUILDER.md (how it works: town and building generators, rules, missions). Every file of the packs:
  docs/ASSET_INDEX.md.
- Changing an existing level (by hand, as an agent, or live in the running game): docs/LEVEL_EDITOR.md (`pakal-level`).
  The standalone editor (gizmos, inspector, prefabs, Play): docs/EDITOR.md (`PakalPlayer <module> --edit <level.json>`).
- The TopDown module (game_genres/topdown, what top-down games share): docs/TOPDOWN_MODULE.md.
- The Zombies module and its debug variables: docs/ZOMBIES_MODULE.md. Downtown levels: docs/ZOMBIES_DOWNTOWN.md.
- Android: docs/ANDROID.md. Network play (ENet, a headless server, snapshots): docs/NETWORK.md.
- Documentation in this repository is in English.
