# Reloading UI and data files while the game runs

A screen's look, its texts and a game's tuning values live in files (`.rml`, `.rcss`, the localization sheet, data `.json`),
never in code (C-DAT-1..5 in [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)). In dev builds and Android `--tools` builds those files also reload
while the game runs: save the file and the game shows the change, without a restart. This is C-DAT-4.

## When it is on

| Build | File reload |
|---|---|
| Desktop (`cmake --preset win64`, Linux) | on |
| Desktop with `-DPAKAL_FILE_RELOAD=OFF` | compiled out (a build that goes to players) |
| Android `build_apk.py --tools` | compiled in (`PAKAL_ENGINE_TOOLS`) |
| Android without `--tools`, `--release`, a flavor (`PAKAL_LOCKED`) | compiled out |

At run time `PAKAL_RELOAD=0` turns it off, and so does `PAKAL_RELEASE=1` (what `build_apk.py --release` writes into a players'
copy). The switch is `PAKAL_FILE_RELOAD` in `source/FileWatcher.h`; `ResourceManager::reload_enabled()` says whether it is on.
When it is off nothing is watched, no thread starts, and every call below does nothing (`watch` returns 0).

## How it works

- `source/FileWatcher.h`: a worker thread looks at each watched file's time and size every 0.5 s (portable
  `std::filesystem` polling, no OS-specific API). A change counts once the file stayed the same for one more look, so an
  editor that writes a file in steps is seen once. The callbacks never run on the worker: they wait for `dispatch()`.
- `ResourceManager::watch(path, callback)`: a file by the path the game reads it with (`"Assets/game/tuning.json"`). It is
  found where `read_bytes` finds it on the disk (as given, then under the root folder). When it changes, the resource
  manager forgets what it kept of it (its bytes, whether it exists), so reading it again in the callback reads the new file.
- The engine calls `ResourceManager::dispatch_changes()` at the start of every update (`Engine::on_update`): **callbacks run on
  the engine's thread**, the one the game states update on and the one games load and change their documents from.
- `ResourceManager::terminate()` (the end of `Engine::run`) drops every watch and stops the worker.

```cpp
// Any file, any thread; the callback runs on the engine's thread. Returns 0 when reload is off.
unsigned ResourceManager::watch(const std::string& path, WatchCallback callback);
void ResourceManager::unwatch(unsigned id);
bool ResourceManager::reload_enabled() const;

// IUIManager: a document read again from its files, in place (the game's thread)
bool IUIManager::reload_document(unsigned id);
void IUIManager::reload_documents();
```

No RmlUi type appears in these headers (ADR-0017): games use `ResourceManager` and `IUIManager` only.

## What reloads

### Documents and style sheets: automatic

Every document loaded with `IUIManager::load_document` is watched together with every file RmlUi read while loading it: the
`.rml`, the `.rcss` it links (also the ones a shared theme links), its templates. RmlUi's style sheet and template caches are
emptied before each load (`Rml::Factory::ClearStyleSheetCache`, `ClearTemplateCache`), so the files are really read, then and
at every reload. When one of them changes, the document reloads in place:

- the same id; shown or hidden as it was; stacked as it was among the game's documents;
- the data models stay (they live outside the documents): the new document binds to them as it loads, with their current
  values, and its `data-event-*` keep reaching the game;
- the click listeners (`add_click_listener`) move to the new document, with the same ids;
- the focus goes back to the element with the same id, when it had the focus and has an id;
- what the game set on its elements through `IUIManager` (`set_text`, `set_markup`, `set_class`, `set_property`,
  `set_attribute`) is put back, the last value of each;
- a document that fails to load leaves the old one on the screen (and logs `[UI] document ... did not reload`).

A style sheet several documents link (a theme) reloads all of them. Fonts and images do not reload: they are cached by
RmlUi and the GPU; restart for a new font or image.

### Localization: games opt in

The documents take their texts as they load (a key in the `.rml` shows the table's text). A game that reads its strings from
a file watches that file, re-reads it, calls `set_strings` again and reloads the documents:

```cpp
// a game's text sheet: after set_strings
ResourceMgr.unwatch(m_watch);
m_watch = ResourceMgr.watch(kSheet, [this, ui](const std::string&)
{
    use(m_language, ui);        // reads the sheet again, set_strings
    ui->reload_documents();     // the documents take the new texts
});
```

Texts the game itself puts into a data model (`UiText::text("KEY")` written into a variable) show the new text the next time
the game sets that variable.

### Data `.json`: games opt in

The same call, and the callback reads the file again. A game's tuning file, for example:

```cpp
// Tuning.cpp; the level state's on_initialize calls Tuning::watch()
void Tuning::watch()
{
    ResourceMgr.unwatch(g_watch);
    g_watch = ResourceMgr.watch("Assets/mygame/game/tuning.json", [](const std::string&) { stored() = read(); });
}
```

What reads the values each time (`Tuning::get().revive.price`) takes the new ones at once; what was made from them before
(a zombie already spawned) keeps the old ones. A game decides per file what "reload" means: re-read and replace a struct,
re-fill a data model (`set_data`), rebuild a table.

Watch from the game's thread (a state's `on_initialize`), keep the id, and `unwatch` it before what the callback uses goes
away. Watching again after `unwatch` is cheap; the watches end with the engine anyway.

## Where the files are

- **Desktop**: the game reads its files from the build folder (`build-win64/bin/Assets/...`, copied from the sources at build
  time). Edit the file there to see it at once; copy it back to the source folder to keep it (or edit the source and rebuild
  the copy step). Never commit an edit made only to the build folder.
- **Android `--tools`**: the game reads from the APK, but a file under the app's storage (its root folder, `files/`) is read
  first. A file that is only in the APK is watched at that place, so a copy pushed there later counts as a change:

  ```
  adb push menu.rcss /data/local/tmp/
  adb shell run-as <package> mkdir -p files/Assets/mygame/ui
  adb shell run-as <package> cp /data/local/tmp/menu.rcss files/Assets/mygame/ui/menu.rcss
  ```

  This needs a debuggable package for `run-as`; `PAKAL_EXTRACT=1` in `pakal_env.txt` extracts every file there instead. Not
  yet tried on a device (see "Not done yet").

## The C-DAT scan

`tools/cdat_scan.py` reads a game's C++ sources (`.cpp`, `.h`...; not `tests/`, `external/`, `build*`) and reports what goes
against C-DAT-1 and C-DAT-3, as `file:line: [rule] message | source`:

| Rule | Flags |
|---|---|
| `element-creation` | `CreateElement`, `AppendChild`, `InsertBefore`, `ReplaceChild`, `RemoveChild`, `SetInnerRML`; `set_markup` with markup written in the code |
| `markup-string` | markup in a string literal (`"<div ...>"`, `"<span>"`) |
| `inline-style` | `set_property`/`SetProperty` with a color, size or position property (`width`, `left`, `color`, `font-size`...); a `style` attribute |
| `color-literal` | `"#rrggbb"`, `"rgb(...)"`/`"rgba(...)"` in a string; an 8-digit hex number (`0xAARRGGBB`/ABGR), except a mask after `&` |
| `player-text` | a string literal with words given as `set_text`'s text (not a localization key `SOME_KEY`, not a call's arguments) |

Comments are not scanned. A line is waived by a comment with a reason, on it or alone on the line above it:

```cpp
m_ui->set_text(kDocument, "fps", "FPS");   // cdat-ok: debug overlay, listed in DESIGN.md (C-DAT-5)
```

A waiver without a reason waives nothing. Waive what really is an exception (C-DAT-5: a debug overlay, an editor's gizmo
colors) or not a color at all (a hash); fix the rest.

```
python tools/cdat_scan.py examples/zombies_module game_genres/topdown  # findings and counts; exit 1 when there are any
python tools/cdat_scan.py --summary --report-only examples/zombies_module # counts only, exit 0
python tools/cdat_scan.py --json examples/zombies_module
```

From Python, `scan(paths) -> list[Finding]` (`path`, `line`, `rule`, `message`, `text`) and `counts(findings)`: what
`tools/release_check.py` registers as a source check. The scan's own tests: `tests/test_cdat_scan.py`
(ctest `CdatScanTests`).

The scan is a heuristic: it finds what is written plainly. A text built with `snprintf("Wave %d")` and then set, or a color
kept in a float array, passes; review (C-DAT-2) still applies.

## Not done yet

- Android: the watcher is compiled into `--tools` builds and watches the override place under the app's storage, but it has
  not been tried on a phone (no Android build folder was at hand). What to check: a pushed `.rcss` reloads the screen; the
  polling thread's cost while the game runs (a few dozen `stat` calls every 0.5 s).
- Fonts and images do not reload.
- Lua scripts are not scanned by the C-DAT scan, nor reloaded here.
