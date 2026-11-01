#!/usr/bin/env bash
# Intel Iris Xe on this hybrid Linux desktop; Ultra is the default quality.
set -euo pipefail
cd "$(dirname "$0")/.."
export PAKAL_GPU=integrated
export PAKAL_BGFX_RENDERER=vulkan
export PAKAL_QUALITY=ultra
export PAKAL_VSYNC=${PAKAL_VSYNC:-0}
export PAKAL_FPS_CAP=${PAKAL_FPS_CAP:-0}
export PAKAL_RESOLUTION=${PAKAL_RESOLUTION:-1280x720}
export PAKAL_RENDER_HEIGHT=${PAKAL_RENDER_HEIGHT:-0}
export PAKAL_FSR=${PAKAL_FSR:-0}
# Restrict device discovery, and keep a failed Vulkan init from falling back to NVIDIA.
intel_icd=/usr/share/vulkan/icd.d/intel_icd.json
[[ -r "$intel_icd" ]] || { echo "Intel Vulkan driver not found: $intel_icd" >&2; exit 1; }
export VK_DRIVER_FILES="$intel_icd"
export VK_ICD_FILENAMES="$intel_icd"
export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
unset LIBGL_ALWAYS_SOFTWARE
# This XWayland/NVIDIA session cannot present Intel DMA-BUFs directly. Copy presentation
# still renders on Intel hardware. Set MESA_VK_WSI_DEBUG='' to retry direct presentation.
export MESA_VK_WSI_DEBUG=${MESA_VK_WSI_DEBUG-sw}
exec ./tools/run_zombimex_linux.sh "$@"
