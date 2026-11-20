# QA hooks: driving a running game from a program

The engine's MCP server (docs/PROFILING.md) also carries the QA hooks (source/mcp/QaTools): what a program needs to
play any game the way a person would and to check where it is. They replace adb taps and screencaps on the phones and
SendInput on the desktop: a scripted smoke story per game can run on every build.

## Turning them on

They come with the engine's tools (`PAKAL_MCP_PORT`; on Android only a `build_apk.py --tools` build), and only when:

- the server requires a token: `PAKAL_MCP_TOKEN=<token>` (without it the measuring tools still answer, the QA hooks are
  not added and the log says so);
- the build is not a release: `PAKAL_RELEASE=1` (what `build_apk.py --release` writes) leaves them out. An Android
  release has no `source/mcp` at all (`--release` refuses `--tools`).

```
set PAKAL_MCP_PORT=8793
set PAKAL_MCP_TOKEN=t
build-win64\bin\PakalPlayer.exe ZombiesModule.dll
python tools/pakal_device.py --port 8793 --token t state
```

On a phone: `pakal_device.py --target <serial> env --package <package> PAKAL_MCP_PORT=8765 PAKAL_MCP_TOKEN=<token>`, start
the app, then every command with `--target <serial> --token <token>` (it sets up `adb forward`).

## The tools

| Tool | Arguments | Does |
|---|---|---|
| `input.key` | `key` (A..Z, Num0..Num9, Escape, Return/Enter, Space, Left, Right, Up, Down, Tab, F1..F15, LShift...), `action` (`tap` default, `down`, `up`), `hold_ms` (80), `wait` (true) | A keyboard key: held for `is_key_pressed` (whatever window has the focus) and, on the desktop, `event_key_down/up` |
| `input.pad` | `seat` (0..3), `button` (A, B, X, Y, LB, RB, Back, Start, LS, RS, Left, Right, Up, Down, LT, RT), `action`, `hold_ms`; or `stick` (`left`/`right`), `x`, `y` (-1..1, y up), `ms` | A gamepad of a seat: its buttons read as the keys the game bound them to (`set_pad_bindings`), its sticks as axis device 0 (axes 0-1 left, 2-3 right), the left stick past half its way as Left/Right/Up/Down |
| `input.touch` | `action` (`tap` default, `down`, `move`, `up`, `swipe`), `x`, `y`, `normalized`, `finger` (0), `hold_ms`; swipe: `to_x`, `to_y`, `ms` (300) | A finger. On a phone it goes the way a real finger goes (the interface with its tap slop, the development panels, the sticks, the game's own touches); on the desktop as SFML gives a touch: the mouse, its button the finger |
| `input.pointer` | `action` (`click` default, `down`, `up`, `move`, `wheel`), `x`, `y`, `normalized`, `button` (left, right, middle), `delta` | The mouse (on a phone: finger 0's touch) |
| `state` | | The game state stack, top first, each state's name and status line (`BaseGameState::describe`); the game's line (`IPakalApplication::describe`); the UI documents loaded (id, source file, title, visible); the time scale; `fresh` (false when the game did not update to answer) |
| `log.tail` | `lines` (50), `filter` (any case), `level` (lowest kept: debug, info, warning, error), `after` (a line number) | The log's last lines, kept in memory from the server's start (the last 2000). `last` in the answer is the newest line's number: pass it as `after` to follow |
| `time.scale` | `scale` (0..4) | Sets or answers how fast the simulation runs (below) |
| `quit` | | Ends the game as when the person closes it |

Coordinates: `x`, `y` are pixels of the window (the touch screen's on a phone), or 0..1 of it with `normalized: true`.
Every input tool answers once the platform delivered its events (`"delivered": true`; a tap's release `hold_ms` after the
press), or at once with `wait: false`.

### How the input reaches the game

Synthetic input is queued on `IInputManager` (`queue_synthetic`) from the server's thread and delivered by
`OSManager::process_window_events`, right after the platform's own events, on the same thread and through the same path:

- keys and pad inputs become held states that each platform's `is_key_pressed` adds (`synthetic_pressed`, with the game's
  pad bindings), and synthetic sticks become axis device 0 while they are pushed;
- on the desktop (SFML), keys, the mouse and touches fire `event_key_*` and `event_mouse_*` as SFML's events do, so the
  RmlUi screens (BgfxRmlUi listens to them) and the game see the same thing;
- on Android, a touch or click runs the touch screen's own code (`TouchInputManager::begin_touch/move_touch/end_touch`):
  `IUIManager::touch` for the interface, the sticks, `event_touch_*` for the game's own touches. Keys are held states, as
  the system's Back is there.

A game cannot tell it from a person's input; nothing in a game changes to be driven.

### The state a game describes

`BaseGameState::describe()` and `IPakalApplication::describe()` are optional (empty by default) and return one short line.
They are called on the game's thread between two updates. For example, a menu may say `screen=main|missions|shop` (plus `options`), and a level
`mission=<id> loading|load_failed|playing|paused|complete|failed`.

### What time.scale covers

`set_sim_time_scale` (source/TimeScale.h) multiplies what `SimulationClock::frame_seconds` returns. Every game on the
fixed-step clock follows it: the TopDown genre's `LevelRuntime` (the brawler and dungeon
prototypes) and the Zombies example level, their simulation steps, animations and effects (0 stops them; the interface,
the menus and the picture keep running). It does not reach what counts the engine's milliseconds itself (the
`on_update(dt)` of menus and of the older 2D genres), Box2D's own steps, nor
the music.

## tools/pakal_device.py

```
pakal_device.py key Escape [--action down|up|tap] [--hold 80]
pakal_device.py tap 0.2 0.93            (0..1 of the screen; --px for pixels; --finger N)
pakal_device.py touch down|move|up X Y
pakal_device.py swipe X Y TO_X TO_Y [--ms 300]
pakal_device.py click X Y [--button right]
pakal_device.py pad A [--seat 1]        pad --stick left 1 0 --ms 500
pakal_device.py state [--json]
pakal_device.py log [--lines 50] [--filter text] [--level warning] [--follow]
pakal_device.py timescale [0..4]
pakal_device.py quit
pakal_device.py story games/<game>/qa/smoke_story.json [--out DIR]
```

## Smoke stories

A story is a JSON file of steps run in order; the first one that fails stops it (exit code 1). It writes
`results.json` and the captures (TGA, and PNG when Pillow is installed) to `--out` (a temporary folder by default).

```json
{"name": "...", "steps": [
  {"comment": "the menu is up", "expect_state": {"top": "ZombiesMenu", "status": "screen=main", "document": "menu.rml", "timeout": 30}},
  {"tap": [0.20, 0.93]},
  {"expect_state": {"status": "screen=missions"}},
  {"screenshot": "missions"},
  {"key": "Escape"}, {"wait": 1}, {"quit": true}
]}
```

| Step | Does |
|---|---|
| `{"wait": s}` | Waits |
| `{"key": name, "action", "hold_ms"}` | `input.key` |
| `{"tap": [x, y], "px", "finger", "hold_ms"}` | `input.touch` tap (0..1 unless `px`) |
| `{"click": [x, y], "button"}` | `input.pointer` click |
| `{"swipe": [x, y, to_x, to_y], "ms"}` | `input.touch` swipe |
| `{"pad": button, "seat"}` / `{"stick": "left", "x", "y", "ms"}` | `input.pad` |
| `{"timescale": s}` | `time.scale` |
| `{"screenshot": name}` | The engine's `screenshot`, into `--out` (desktop paths; on a phone use a `call` with a path the app can write) |
| `{"expect_state": {...}}` | Retried until `timeout` (5 s): `top` is the top state's name, `status` is text in its status line, `document` is text in the source of a visible UI document, `states` are names on the stack |
| `{"expect_log": {"filter", "timeout"}}` | A log line containing it, since the story started |
| `{"call": tool, "arguments": {...}}` | Any tool |
| `{"quit": true}` | `quit` |

A smoke story usually walks the main menu, starts the first level, waits until it plays, pauses and resumes with Escape,
and quits.

## Security

Loopback only, as every engine tool (docs/PROFILING.md). The QA hooks drive the game, so they need the token on every
request and are never added to a release (`PAKAL_RELEASE=1`) or compiled into an Android build without `--tools`. The
decision is `qa_tools_allowed` (source/mcp/QaTools.cpp), made by `start_engine_tools`; tests/QaToolsTests.cpp checks it.
