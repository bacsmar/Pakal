# Build identity and the build label

Every copy of a game says which copy it is, so a capture, a log or a finding always names the build it came from. It serves
C-ID-7 and C-BLD-1 in [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).

## What a build carries

`build.json` holds the identity:

```json
{ "version": "1.0.0-beta.10", "channel": "store", "build": 530, "commit": "7f0e1aa", "branch": "main",
  "dirty": false, "code": 10010, "edition": "demo mvp", "label_flag_url": "https://..." }
```

| Field | Meaning |
|---|---|
| `version` | The game's version, from its version file (android.json `version_file`). |
| `channel` | `dev`, `qa` or `store`. Set by the build, never typed by hand (below). |
| `build`, `commit`, `dirty` | The build number, the short commit, and whether the working tree had uncommitted changes. |
| `code` | The store's version code. |
| `edition` | The flavor's edition (android.json flavor `"edition"`, else the flavor's name). Only for flavors. |
| `label_flag_url` | See Production, below. Only in store builds of games that set it. |

Where it is written:
- **Android:** `android/build_apk.py` writes it in the package as `Assets/build.json`, and the same data beside the package
  as `<package>.build.json`.
- **Desktop:** every build writes `build.json` next to the programs (`build-win64/bin`). It has channel `dev`, the
  commit and the dirty mark (`cmake/WriteBuildIdentity.cmake`). A packager for a desktop release writes its own, with
  its channel, in the package's `Assets/build.json` or next to the executable.

The engine reads `Assets/build.json` first, then `build.json` next to the executable. With neither, it is an unknown
`dev` build.

## The channel

| Channel | `build_apk.py` | What it is |
|---|---|---|
| `dev` | a plain build | a developer's copy |
| `qa` | `--tools`, `--cheats` or `--side` | a tester's copy |
| `store` | `--release` (a flavor included) | the signed store build, on any test track |
| (none) | — | production: a promoted store bundle (see Production) |

The track is not in the label: the store promotes one bundle from internal to closed to production.

## The log and the label

- The engine logs the identity as its first line: `[Build] v1.0.0-beta.10-store  530.7f0e1aa (branch main), version code 10010`.
- The engine draws the label itself, so no game can forget it:
  - **Format:** `<edition> v<version>-<channel>  <build>.<commit>[.dirty]`. A build that doesn't know its version shows the
    channel alone, for example `dev  0.98fc21e2.dirty`.
  - **Where:** a corner, over every document. It never takes a click or the focus. It uses the first font the game loads
    (`IUIManager::load_font`). Until then, or in a game with no interface, it is drawn as debug text in the same corner.
  - **The corner:** bottom left by default. The game moves it to the corner of its version text, never over a button:
    `engine->set_build_label_corner(BuildLabelCorner::TopRight)`.
- **API:** `Engine::build_identity()` gives the fields, `BuildIdentity::label()` the text, and `Engine::build_label_shown()`
  whether it is drawn.

## Production

Production gets no build of its own. The last `store` bundle the testers played is promoted as it is, and a flag that
does not change the bundle hides its label:

1. The game's `android.json` names the flag: `"label_flag_url": "https://<host>/<game>/label_flag.json"`. A store build
   carries it in its `build.json`.
2. At start, a store build asks that address, with a 3-second timeout. Its label waits for the answer.
3. The answer lists the version codes promoted to production: `{"hide_label": [10010]}`. When its own code is listed,
   the label stays hidden. Any other answer shows the label: no network, an error, a broken file, or a code that isn't
   listed.
4. To promote, whoever releases adds the bundle's version code to the file. Nothing is rebuilt.

The file is public data, with no secret in it. Any static host that serves HTTPS works.
