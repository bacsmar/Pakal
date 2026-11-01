# ZombiMex on Linux with Intel Iris Xe

Measured 2026-10-31 on *A Walk on the Malecón* at (-72.1, -74.7), under the tree canopies. Isolated test profile, seed 2026,
Sam/Glock, 222 actors, no enemies. This is not a horde test or a guarantee for the whole campaign.

## Result (Ultra, every effect on: SSAO, tilt-shift, shadows, lightmap, GI; FSR off, vsync off, no FPS cap)

| Mode | Mean FPS | p50 | p95 | p99 | Max |
|---|---:|---:|---:|---:|---:|
| 1920x1080 native | 55.13 | 18.01 ms | 20.53 ms | 22.05 ms | 24.17 ms |
| 1280x720 native | 86.60 | 11.55 ms | 14.07 ms | 15.25 ms | 20.88 ms |

40 s measured after 30 s of warm-up. 720p is faster only because it draws fewer pixels. 1080p misses the 45 FPS budget
(22.22 ms) at p95/p99 in this capture. The monitor runs at ~60 Hz, so FPS above that counts produced frames only.

## Running on Intel

```bash
./tools/run_zombimex_intel_linux.sh                                  # Ultra, 1280x720, Intel/Vulkan
PAKAL_RESOLUTION=1920x1080 ./tools/run_zombimex_intel_linux.sh
PAKAL_FPS_CAP=60 ./tools/run_zombimex_intel_linux.sh
```

The launcher restricts Vulkan to the Intel ICD so a failed init cannot fall back to NVIDIA. On a hybrid XWayland/NVIDIA
session, Intel cannot present DMA-BUFs directly (neither Vulkan nor Mesa OpenGL swapchains worked), so it sets
`MESA_VK_WSI_DEBUG=sw`: rendering and shaders still run on the Intel GPU, only presentation is a copy. Set
`MESA_VK_WSI_DEBUG=''` to retry direct presentation on another session. Verified device: Vulkan, vendor 0x8086, 0x46a6.

## Build changes

- `CMakeLists.txt`: on Linux the engine shaders are compiled for both GLSL and SPIR-V (Windows keeps Direct3D).
- `BgfxGraphicsSystem.cpp`: the RGB8 back-buffer format of the X11 visual is requested for OpenGL only.
- bgfx: the compute-barrier fix is `patches/bgfx/0001-gl-compute-vertex-barrier.patch` (already tracked, unchanged).

## Where the frame goes (1080p, median GPU timers)

Opaque 10.62 ms (faded-by-dither foliage is drawn here), full post-process 3.36 ms, shadows 0.61 ms, blended transparent
0.26 ms, UI 0.08 ms; GPU frame 14.82 ms. CPU: sim.step ~1.04 ms, gfx.meshes ~0.86 ms; gfx.submit (~17 ms) is waiting on
the GPU/driver/presentation.

## Discarded experiments (1080p Ultra, short noisy runs)

| Experiment | Mean FPS | p95 | Decision |
|---|---:|---:|---|
| Vulkan/Intel reference | 55.22 | 20.65 ms | keep |
| Rotating SSAO/tilt-shift samples | 55.01 | 20.86 ms | no gain, reverted |
| Depth prepass for faded foliage | 43.46 | 26.51 ms | worse (~5.2 ms extra), reverted |
| Instancing off | 52.60 | 23.17 ms | keep instancing |
| Linear software presentation | 54.82 | 21.29 ms | no gain |
| Sort by program instead of depth | 52.93 | 21.83 ms | keep depth sort |
| RGBA8 atlas mipmaps | 54.61 | 20.99 ms | not enabled |
| OpenGL via Zink on Intel | 56.77 | 21.76 ms | worse p95, keep Vulkan |

No same-resolution shader improvement was found; the deliverable is a reproducible Intel/Vulkan path with valid numbers.
Validated on Linux only (FrameTimingTests, ZombiMexMetaTests pass); not re-validated on Windows or Android.
