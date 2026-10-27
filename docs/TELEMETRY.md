# Telemetry

What a run costs, measured while it plays (`source/Profiler.h`, in the core).

## Turning it on

| Variable | Effect |
|---|---|
| `PAKAL_PROFILE=<file>` | One JSON object per line (NDJSON): a record each second and a summary at the end. `1`: only the log lines |
| `PAKAL_PROFILE_SKIP=<s>` | The first seconds (the loading) are left out of the summary |
| `PAKAL_PROFILE_SECONDS=<s>` | The program ends after that long, with the summary written |
| `PAKAL_FPS_LOG=1` | The older per-second log lines (draws, GPU views); it also turns the profiler's log on |

On Android the variables go in `pakal_env.txt` (see ANDROID.md), and the file in the app's external folder
(`/sdcard/Android/data/org.pakalengine.zombies/files/`), where adb can pull it.

## What a record has

- `fps` and `frame_ms`: the frame rate and the frame times' p50, p95, p99 and maximum (from one drawn frame to the next).
- `sections`: the time of each section of code per drawn frame, its worst single time and how many times it ran. Code marks
  a section with `PAKAL_PROFILE_SCOPE("name")` (any thread; off, it costs one test of a flag). Today: each system of the
  engine's main loop by its name (the graphics system's includes the game's update and the wait for the frame),
  `gfx.frame` (the graphics thread's work) and within it `gfx.meshes` (the models' poses and draws), `gfx.pose` (the
  poses alone), `gfx.ui` (the interface) and `gfx.submit` (`bgfx::frame`: the frame handed to bgfx's render thread,
  waiting for the one before: high when the GPU or the driver is the limit), Zombies' `sim.step`, `sim.physics` and
  `level.views`, and the dungeon's `sim.step`, `dungeon.frame` and `net.poll`.
- `counters`: values the subsystems set (`Profiler::counter`): the process's memory (`mem.process_mb`), the GPU's
  (`gpu.textures_mb`, `gpu.targets_mb`), draws, the render size, the level's load time, the game's actors and enemies, and
  on Android the device's thermal status (`thermal.status`, Android 10 and later: 0 none, 1 light, 2 moderate, 3 severe...;
  -1 where the system does not tell it).
- `info` (once, and again when it changes): the platform, the device's model, the quality, the level.

## Tools

- `tools/pakal_prof.py <run.ndjson> [<other.ndjson> ...] [--seconds]`: a run's summary, or several side by side.
- `tools/pakal_bench.py [--device windows|android|<serial>|all] [--seconds 60] [--skip 10]`: runs the Zombies bot on the
  heaviest level (Downtown) with the telemetry on, on this computer and on the devices connected by adb (kept awake), and
  compares each with its budget in `tools/bench_budgets.json`, or in a `bench_budgets.local.json` next to it (not in Git)
  that lists one's own devices (its quality, frame rate, p95 and p99). Exits 1 when one
  misses it. The runs' files stay in `build-win64/bench`.

Reading a run: a high `gfx.submit` with the rest low is the GPU (or the driver); run again with a smaller picture
(`PAKAL_RENDER_HEIGHT`) or without FSR (`PAKAL_FSR=0`): if the frame rate goes up, the pixels are the cost. On a computer
with two GPUs `PAKAL_GPU=integrated` shows the cost better than a dedicated one that has plenty to spare.
