# Measuring and tuning a running game

A running game can be measured and tuned while it plays, on this computer or on an Android device, without touching its
window: the game serves the engine's tools on its MCP server (JSON-RPC 2.0 over HTTP, `POST /mcp`), on 127.0.0.1 only.
Whatever speaks HTTP reaches them: `tools/pakal_device.py` (for a person or an agent, in a shell), the level editor's
Device window (graphs and sliders, docs/EDITOR.md), or `curl`.

## Turning it on

- **Desktop**: `PAKAL_MCP_PORT=8765` in the environment of PakalPlayer (and `PAKAL_MCP_TOKEN=<token>` to require
  `Authorization: Bearer <token>`). Off without the port.
- **Android**: only a build that asks for it has it: `python android/build_apk.py --game <game> --tools` compiles the
  tools in (`PAKAL_ENGINE_TOOLS`) and gives the app the INTERNET permission (a socket needs it, even on 127.0.0.1). A
  build without `--tools` (any release) has neither. Then `PAKAL_MCP_PORT=8765` in the app's `pakal_env.txt` turns it on
  at the next start:

  ```
  python tools/pakal_device.py --target <serial> env --package <package> PAKAL_MCP_PORT=8765
  ```

  The computer reaches it through `adb forward tcp:8765 tcp:8765` (pakal_device.py sets it up for a `--target`): the port
  stays the phone's loopback, nothing listens on its network.

While the server runs, the profiler measures live (`Profiler::set_live`): every second's record is kept in memory (the
last 600), and the renderer times each of its passes on the GPU.

## The tools

| Tool | Does |
|---|---|
| `get_status` | Engine and graphics frame rates |
| `get_stats` | The last second as the profiler records it (below) |
| `get_profile` `{after}` | The records kept whose `clock` is after `after` (seconds since the game started; -1: all) |
| `record_start` `{path}` / `record_stop` | A recording into a file of the game's own (PAKAL_PROFILE's format, its summary at the end) |
| `get_settings` | The quality level, every preset's fields, post-process, shadows, render scale, FSR, sharpness, the governor (and its status), the frame's size |
| `set_settings` | Any of `quality`, `preset` (`{field: value}`, of the quality given or the current one), `post`, `shadows`, `renderScale` (0.3..1), `fsr`, `sharpness`, `governor`, `gpuProfiler`; applied now, answers the settings after |
| `set_camera` `{eye, look}` / `{release: true}` | Holds the camera at a pose (render space: x east, y up, z south; meters) whatever the game does with it, to compare settings on one picture |
| `screenshot` `{path}` | The next frame as a TGA (a path the game can write) |

A record (`get_stats`, `get_profile`, the recordings) is the profiler's: `t`, `clock`, `fps`, `frame_ms` (`p50`, `p95`,
`p99`, `max`), `sections` (each `ms_per_frame`, `worst_ms`, `calls`), `counters`, and `info` once. The counters the
renderer adds: `gpu.frame_ms`, `gpu.view.<pass>_ms` (`main`, `transparent`, `shadow`, `post`, `bloom`, `ao`, `fsr_easu`,
`fsr_rcas`, `ui`, `debug_ui`...), `gpu.draws`, `gpu.primitives`, `gpu.textures_mb`, `gpu.targets_mb`, `cpu.submit_ms`,
`render.width/height/scale/quality/fsr`, `governor.on`, `governor.cap_fps`; and the profiler's `mem.process_mb` and, on
Android, `thermal.status`.

A game adds its own tools to the same server (`McpServer::active()->add_tool`): what only it knows, such as its data
files. A `write_back` tool (`{settings}`: what get_settings answers) is what the editor and pakal_device.py call to keep
tuned values in the game's data; the engine has none.

## tools/pakal_device.py

```
pakal_device.py targets                               the local game (if it answers) and the devices adb sees
pakal_device.py --target <serial> forward             adb forward of the port
pakal_device.py --target <serial> env --package <p> KEY=VALUE   lines of the app's pakal_env.txt
pakal_device.py [--target T] tools | stats
pakal_device.py [--target T] call <tool> ['<json>' | @file.json]
pakal_device.py [--target T] set post.aoStrength 0.5  (preset.<field>, shadows.<field>, renderScale, fsr, quality...)
pakal_device.py [--target T] record 60 run.ndjson     then: python tools/pakal_prof.py run.ndjson
pakal_device.py [--target T] ab renderScale 1.0 0.67 --seconds 10 --rounds 3
pakal_device.py [--target T] writeback [--map map.json] [--settings tuned.json]
```

- `--port` (8765, or PAKAL_MCP_PORT) and `--token` (PAKAL_MCP_TOKEN). adb comes from `ADB`, `ANDROID_HOME` or
  `ANDROID_SDK_ROOT`, else the PATH. The tool lists every device adb sees: which ones may be used is the person's rule.
- `record` writes each second's record as it comes (the first with `info`: the target, quality, render scale, FSR), the
  format `tools/pakal_prof.py` and `tools/pakal_bench.py` read.
- `ab` sets the two values in turn (each `--settle` seconds to settle, then measured `--seconds`), `--rounds` times, and
  prints fps, frame p50 and p95, GPU time, draws, triangles and each pass for A, B and B - A; then puts the setting back.
- `writeback` hands the settings (the game's now, or a file) to the game's `write_back` tool; with `--map`, writes them
  into JSON files itself: `{"fields": {"<setting>": {"file": "<path relative to the map>", "pointer": "/json/pointer"}}}`.
- Its HTTP is a plain socket read to its end, the body taken whole or chunked: a local proxy (some antivirus web
  shields) may rewrite the headers of loopback HTTP.

## Security

The server binds 127.0.0.1 only; on a phone only adb (USB, the person's authorized computer) reaches it. It is never on
by default: the environment (or `pakal_env.txt`) asks for it, and on Android only a `--tools` build has it at all. A
token, when set, is required on every request.
