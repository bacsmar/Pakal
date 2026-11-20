# Release check (`tools/release_check.py`)

Checks a game's finished Android package, an APK or an App Bundle, before it is released. Its checks carry the codes of
[RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md): the check runs **on the package itself**, not only on the data,
and must exit 0 before a release is handed over.

```
python tools/release_check.py <package.apk|package.aab> [--game <name>] [--channel store|qa] [--last-code N] [--json]
```

- `--game`: the game's folder, `games/<name>` or `examples/<name>`. When it is left out, the tool finds the game from
  the manifest's package (the `package` of a game's `android.json`, or one of its flavors'; a side copy's `.dev` is
  dropped), or else from the file's name (`pakal-<game>[-<flavor>]`).
- `--channel`: `store` (the default) checks a store package. `qa` checks a tester's copy, where cheats, the tools, a debug
  key and loose art are allowed. Without the option, the channel comes from `Assets/build.json`'s `channel` when the
  build wrote one (`dev` counts as `qa`).
- `--last-code`: the last version code released to the store; the package's code must be above it. The game's config
  can hold it too (`last_version_code`).
- `--json`: the results as JSON (`package`, `game`, `flavor`, `channel`, `results`, `counts`, `ok`).

Each result has a requirement ID, a check name, a status and a message:

| Status | Meaning |
|---|---|
| `PASS` | the requirement holds |
| `FAIL` | the requirement does not hold: the exit code is 1 |
| `WARN` | reported, does not fail the run (a SHOULD, a qa package, something not known) |
| `SKIP` | does not apply (a qa package, nothing configured) |
| `WAIVED` | would fail or warn, but the game's config exempts it (with the reason) |

The tool reads only the package, `<package>.build.json` beside it, and the game's `android.json` and
`release_check.json`. It never opens a keystore, a password or a secret file. The certificate is checked by
comparing the SHA-256 the package declares with the ones the game's config lists: a certificate's SHA-256 is public.
When the secret scan finds something, it reports the file and the kind of secret, never what matched.

## The engine's checks

| Check | ID | What it checks |
|---|---|---|
| `debuggable` | C-BLD-2 | The manifest's `<application>` is not `android:debuggable`. |
| `certificate` | C-BLD-4 | The package is signed (APK: the v2/v3 signing block, else the v1 signature; bundle: `META-INF/*.RSA`), and every signer's SHA-256 is in the config's `certificates` or the flavor's `signers` (`android.json`). Without any expected certificate, a debug key (`Pakal Debug`, `Android Debug`) fails and any other key warns. Skipped for qa. |
| `version` | C-BLD-1 | The manifest's version code and name are the ones in `Assets/build.json`, and the ones the game's `version_file` gives (`build_apk.py`'s `version_code`). |
| `build-identity` | C-BLD-1 | `<package>.build.json` beside the package is the same as `Assets/build.json` (a warning when it is missing). |
| `dirty` | C-BLD-1 | The build was not made from uncommitted changes (a warning for qa). |
| `channel` | C-ID-7 | When `build.json` has a `channel`, it is the channel being checked. |
| `version-code` | C-BLD-4 | The code is above the last released one (`--last-code` or `last_version_code`; a warning when unknown). Skipped for qa. |
| `release-env` | C-BLD-2 | A store package has `PAKAL_RELEASE=1` in its `pakal_env.txt` (a players' copy). A qa package must **not** have it, so testers never send players' telemetry. |
| `locked` | C-BLD-2 | A store package was built with `PAKAL_LOCKED` (the device's `pakal_env.txt` is not read). The library carries the log line only a locked build has. A warning for now, because `build_apk.py --release` locks only flavors so far. `"require_locked": true` makes it a failure. Skipped for qa. |
| `pack` | C-BLD-11 | The game's `android.json` `"pack"` file is in the package, stored uncompressed (APK), or kept uncompressed by the bundle's config. It opens as a zip, its entries are sealed (they start with `PKP1`, so the tool needs no key), it holds every listed folder, and none of those folders is loose in the package. `"pack": false` with a `"pack_reason"` is WAIVED. A game with no pack fails as a store package and warns as qa. |
| `secrets` | C-BLD-6 | No secret file (`forge.json`, `.env`, `local_paths.json`, `*.keystore`, `*.jks`, `*.p12`, `*.pfx`, `*.pem`, `*.key`, plus the config's `secret_names`). Nothing that looks like a secret in the package's text files: a private key, an AWS key, a keystore password variable, a password or secret in JSON, plus the config's `secret_patterns`. No private key in the native libraries. |
| `cheat-strings` | C-BLD-2 | None of the strings that only a cheats or tools build has is in a native library: the engine's MCP server, plus the config's `native_forbidden`. No `cheats.rml` and none of the config's `forbidden_files` is in the package. A warning for qa. |
| `dev-content` | C-BLD-8 | No JSON file of the package has data flagged `"dev": true`. A warning for qa. |
| `dev-words` | C-BLD-8 | No dev word (dev, debug, test, wip, cheat, TODO in capitals so that Spanish "todo" passes, F1…F9, dev panel) in the player-facing text the config names (`player_text`). A CSV's first column (the keys) and a JSON's keys are not scanned. A warning for qa. Skipped when nothing is named. |
| `size` | C-PRF-5 | The package is within `max_size_mb`. A bundle's base module (compressed) is within `base_max_mb` (default 200, Google Play's limit), and each asset pack within `asset_pack_max_mb`. |

Files sealed in the pack cannot be read without the game's key, and the tool never reads the key. So the secret,
dev-content and dev-word scans of the package see only what is outside the pack. Name the sources in `player_text.source`
to check the text that goes inside it. The standard's other pack checks are left to QA: the key string not in the
package's plain files, and the game loading every level from the pack.

## A game's own checks

A game adds checks without editing the tool. It has two places for them, both optional.

**`<game folder>/release_check.json`**, the game's settings:

```json
{
 "certificates": ["<sha-256 of the upload key's certificate>", "<sha-256 of Play's app signing certificate>"],
 "last_version_code": 10021,
 "max_size_mb": 200,
 "base_max_mb": 150,
 "asset_pack_max_mb": {"gamedata": 400},
 "native_forbidden": ["Assets/mygame/ui/cheats.rml", "[Cheats]"],
 "forbidden_files": ["Assets/mygame/levels/lab_*"],
 "secret_names": ["service.json"],
 "secret_patterns": {"service token": "tok_[0-9a-f]{32}"},
 "secret_allow": ["assets/Assets/mygame/docs/*"],
 "player_text": {"package": ["Assets/mygame/strings/*.csv"], "source": ["game/strings/strings.csv"]},
 "dev_words": ["(?i)\\bdebug\\b"],
 "require_locked": true,
 "exempt": {"pack": "ships loose art for the jam build; release lead, 2026-11-19"},
 "plugin": "tools/release_checks.py"
}
```

- `certificates`: take the SHA-256 from a package the key signed (`keytool -printcert -jarfile <package>`), or from
  Play Console (*Test and release > App integrity*). Never from the keystore.
- `player_text.package` globs match asset paths as the game names them (`Assets/...`). `player_text.source` globs are
  relative to the game's folder.
- `dev_words` replaces the default list (Python regular expressions).
- `exempt` maps a check name to the reason of whoever approved it, as in the game's profile (*Exemptions*). Its FAIL or WARN becomes
  WAIVED.

**`<game folder>/tools/release_checks.py`** (or the config's `plugin`) is a plugin with `run(ctx)`, which runs after the
engine's checks:

```python
def run(ctx):
    ok = ctx.package is not None and 'Assets/mygame/campaign.json' in ctx.package.assets
    ctx.report('C-BLD-4', 'mygame-campaign', 'PASS' if ok else 'FAIL', 'the campaign is packed' if ok else 'no campaign')
```

`ctx` holds:
- `package`: `None` when the package could not be opened. It has `names`, `assets` (asset path to entry), `read(entry)`,
  `read_asset(path)`, `manifest`, `native_libraries()`, `certificates()` and `kind` (`apk`/`aab`).
- `package_path`, `game`, `game_dir`, `flavor`, `config`, `android` (its `android.json`), `channel`, `store`, `root`
  and `args`.
- `strict()`: `FAIL` for a store package, `WARN` for qa.
- `report(id, check, status, message)`.

When the package is missing, the engine's checks report one failure, and the game's plugin still runs, so data checks
that need no package still report.

A game usually keeps a thin wrapper beside its sources (`<game>/tools/release_check.py`): it runs this tool with
`--game <name>` on `build-android/pakal-<name>.apk` (`--apk` for another), and checks it as qa unless `--channel` or
`build.json` say otherwise. Its own checks (its campaign, banned assets, texts, the Android build's cheats) sit in a
plugin next to it, with their strings in a JSON file.

## Checks of the sources (C-DAT)

`SOURCE_CHECKS` in the tool is where checks of a game's **sources**, not its package, plug in
(`register_source_check(fn)`, with the same `fn(ctx)` as a plugin). They run after the package's checks and before the
game's plugin. The C-DAT scan (`tools/cdat_scan.py`) goes there: element creation, inline styles, colors and
player-facing literals in game code. It checks sources, not the package.

## Tests

```
python -m unittest tests/test_release_check.py
```

The tests build tiny fake APKs and bundles: a binary manifest or a protocol buffer manifest, a v2 signing block or a v1
PKCS#7 signature, a sealed pack. They check each check's PASS and FAIL cases, the plugin, the exemptions and the exit
code.
