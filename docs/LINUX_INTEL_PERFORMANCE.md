# Linux on an integrated Intel GPU

How to run Pakal on an integrated Intel GPU (Iris Xe) under Linux, including on a hybrid laptop that also has an NVIDIA
GPU, and what one 3D scene costs there.

## Running on the Intel GPU

These are the player's environment variables. Set them, then start `PakalPlayer` from `build-linux/bin` with a module:

```bash
export PAKAL_GPU=integrated
export PAKAL_BGFX_RENDERER=vulkan
export PAKAL_QUALITY=ultra
export PAKAL_RESOLUTION=1280x720     # or 1920x1080
export PAKAL_VSYNC=0 PAKAL_FPS_CAP=0  # to measure; PAKAL_FPS_CAP=60 to play
# Only the Intel Vulkan driver, so a failed start cannot fall back to the NVIDIA GPU:
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json
export VK_ICD_FILENAMES=$VK_DRIVER_FILES
export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
unset LIBGL_ALWAYS_SOFTWARE
# Hybrid XWayland/NVIDIA session: present by copy (see below)
export MESA_VK_WSI_DEBUG=sw
./PakalPlayer <module>.so
```

On a hybrid XWayland/NVIDIA session the Intel GPU cannot present its buffers directly (neither the Vulkan nor the Mesa
OpenGL swapchain worked). `MESA_VK_WSI_DEBUG=sw` presents by copy: rendering and shaders still run on the Intel GPU.
Set `MESA_VK_WSI_DEBUG=''` to try direct presentation on another kind of session. Verified device: Vulkan, vendor
0x8086, device 0x46a6.

## One measured scene

A 3D town level with 222 actors and dense tree canopies, no enemies, at Ultra with every effect on (SSAO, tilt-shift,
shadows, lightmap, GI), FSR off, vsync off, no frame cap. 40 s measured after 30 s of warm-up:

| Mode | Mean FPS | p50 | p95 | p99 | Max |
|---|---:|---:|---:|---:|---:|
| 1920x1080 native | 55.13 | 18.01 ms | 20.53 ms | 22.05 ms | 24.17 ms |
| 1280x720 native | 86.60 | 11.55 ms | 14.07 ms | 15.25 ms | 20.88 ms |

720p is faster only because it draws fewer pixels. The monitor runs at about 60 Hz, so frame rates above that count
frames produced, not shown. This is one scene, not a guarantee for a whole game.

### Where the frame goes (1080p, median GPU timers)

Opaque 10.62 ms (dithered, faded foliage is drawn here), the whole post-process 3.36 ms, shadows 0.61 ms, blended
transparent 0.26 ms, UI 0.08 ms: a GPU frame of 14.82 ms. On the CPU, `sim.step` takes about 1.04 ms and `gfx.meshes`
about 0.86 ms; `gfx.submit` (about 17 ms) is waiting on the GPU, the driver and presentation.

### Tried and dropped (1080p Ultra, short noisy runs)

| Experiment | Mean FPS | p95 | Decision |
|---|---:|---:|---|
| Vulkan on Intel (reference) | 55.22 | 20.65 ms | keep |
| Rotating SSAO and tilt-shift samples | 55.01 | 20.86 ms | no gain, reverted |
| Depth prepass for faded foliage | 43.46 | 26.51 ms | worse (about 5.2 ms more), reverted |
| Instancing off | 52.60 | 23.17 ms | keep instancing |
| Linear software presentation | 54.82 | 21.29 ms | no gain |
| Sort by program instead of depth | 52.93 | 21.83 ms | keep depth sort |
| RGBA8 atlas mipmaps | 54.61 | 20.99 ms | not enabled |
| OpenGL through Zink on Intel | 56.77 | 21.76 ms | worse p95, keep Vulkan |

No shader change gave more frames at the same resolution.

## What the engine does for Linux

- On Linux the engine shaders are compiled for both GLSL and SPIR-V (Windows keeps Direct3D).
- The RGB8 back-buffer format of the X11 visual is requested for OpenGL only.
- bgfx's compute barrier fix is `patches/bgfx/0001-gl-compute-vertex-barrier.patch`.
