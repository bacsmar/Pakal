# Release checklist

What a finished game built on Pakal should hold before players get it. Each item has a code; the engine's code, its tools
and their docs name the items they serve by these codes, so a search for a code finds everything behind it.
`tools/release_check.py` checks the package against the build items ([RELEASE_CHECK.md](RELEASE_CHECK.md)), and
`tools/cdat_scan.py` checks the sources against the data items ([UI_RELOAD.md](UI_RELOAD.md)).

## Identity (C-ID)

- **C-ID-1:** the game opens on the maker's logo, briefly, before its own screens ([SPLASH.md](SPLASH.md)).
- **C-ID-7:** every copy that is not production says which build it is, in a corner of the screen: its version, channel,
  build number and commit ([BUILD_IDENTITY.md](BUILD_IDENTITY.md)).

## Build (C-BLD)

- **C-BLD-1:** a build knows its identity (version, channel, build number, commit, uncommitted changes), logs it first,
  and the package's version matches it.
- **C-BLD-2:** a store package carries no development tools: not debuggable, no cheats, no MCP server, the players'
  environment (`PAKAL_RELEASE=1`) and a locked configuration.
- **C-BLD-4:** a store package is signed with the game's own key, and its version code is above the last one released.
- **C-BLD-6:** no secret (keys, keystores, passwords, tokens, local configuration) ever goes into a package.
- **C-BLD-8:** no development content or words reach the players: data flagged `"dev"`, "debug", "test", F-key hints.
- **C-BLD-11:** a store package's art is sealed in one pack.

## Options (C-OPT)

Every game offers these in its options ([SETTINGS.md](SETTINGS.md)):

- **C-OPT-1:** music volume, 0–100 in steps of 10.
- **C-OPT-2:** sound effects volume, 0–100 in steps of 10.
- **C-OPT-3:** the language; the first start follows the device's.
- **C-OPT-5:** vibration on or off.
- **C-OPT-6:** telemetry, off until the player agrees.
- **C-OPT-7:** the credits.
- **C-OPT-8:** the privacy policy's address.
- **C-OPT-9:** the version and build number.
- **C-OPT-10:** a reset of the options to their defaults (not of the player's progress).
- **C-OPT-12:** the picture's quality, automatic or a fixed level.

## Pause and saving (C-PAU, C-SAV)

- **C-PAU-3:** the pause panel and the options change the same values, and each shows the other's change at once.
- **C-SAV-1:** every change of the options is saved at once, and a save never leaves a broken file behind.

## Data, not code (C-DAT)

- **C-DAT-1:** interfaces are built in documents and styled in style sheets, not created or styled in code.
- **C-DAT-2:** tuning values live in data files, not in code (checked in review).
- **C-DAT-3:** player-facing text lives in string tables, not in code.
- **C-DAT-4:** in development builds, interface and data files reload while the game runs.
- **C-DAT-5:** an exception (a debug overlay, an editor's gizmo) is marked in the code with its reason.

## Performance (C-PRF)

- **C-PRF-5:** the package fits the store's size limits.
