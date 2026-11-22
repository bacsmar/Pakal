# vcpkg overlay ports

Ports that vcpkg fixed upstream after the manifest's `builtin-baseline`. The CI's Windows build uses them
(`VCPKG_OVERLAY_PORTS` in `.github/workflows/build.yml`) because it builds every package from scratch. A machine whose
packages are already built doesn't need them; one that hits the same download error can set the same variable.

- `kubazip` 0.3.15, port-version 1 (vcpkg, 2026-11-21): GitHub's archive of v0.3.15 changed, and the hash the baseline
  pins no longer matches. Assimp pulls it in.

Remove a port from here when the baseline moves past its fix.
