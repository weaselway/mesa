# Building mesa for weaselway (Nix)

A compile check for this fork, using the dev shell in [flake.nix](flake.nix)
(nixpkgs `nixos-26.05`). Nothing is installed. What ships is built by the
[weaselway](https://github.com/weaselway/weaselway) flake, from the release
branch that matches nixpkgs' mesa (now `mesa-26.2.1-wsl`).

```sh
./weaselway-build.sh
```

This enters `nix develop` on its own, configures `build/nix` on the first run,
and then runs `meson compile`. Later runs only compile; meson reconfigures by
itself when a `meson.build` changes.

- `RECONFIGURE=1 ./weaselway-build.sh` re-applies the flags to an existing
  build dir, for example after editing them.
- `BUILDTYPE=debugoptimized RECONFIGURE=1 ./weaselway-build.sh` switches the
  build type (the default is `release`).
- `rm -rf build/nix` starts from scratch.

## Flags

The list lives in [weaselway-build.sh](weaselway-build.sh).

| Flag | Why |
|---|---|
| `-Dgallium-drivers=softpipe,d3d12` | d3d12 is the WSL GPU driver; softpipe is the fallback |
| `-Dvulkan-drivers=swrast,microsoft-experimental` | dzn (Vulkan on D3D12) plus lavapipe |
| `-Dgallium-d3d12-graphics=enabled`, `-Dgallium-d3d12-video=enabled` | d3d12 GL and VA-API |
| `-Dplatforms=x11,wayland`, `-Degl-native-platform=surfaceless` | as shipped |
| `-Dglvnd=enabled` | the distro uses libglvnd |
| `-Dshader-cache=enabled` | as shipped |

## What comes out

- `build/nix/src/gallium/targets/dri/libgallium-*.so`: the d3d12 gallium driver
- `build/nix/src/gallium/targets/dri/d3d12_drv_video.so`: VA-API
- `build/nix/src/microsoft/vulkan/libvulkan_dzn.so`: dzn

## Notes

- The dev shell takes its dependencies from nixpkgs' own mesa
  (`inputsFrom = [ pkgs.mesa ]`, 26.1.x). The fork's `main` is 26.3-devel. If
  `main` starts needing something newer, add it to `packages` in the flake.
- meson picks the newest `python3.X` on `PATH`, so a python from your user
  profile would win over the shell's python 3.13 and its mako/yaml. The shell
  therefore provides its own python 3.14 with those modules. If meson reports
  a missing python module, check which `Program python3.X found` it chose.
- On an aarch64 host this builds for arm64. The d3d12 code is
  architecture-independent, so the compile check still holds.
- The weaselway image builds a `mesa-X.Y.Z-wsl` release branch, not `main`.
  `main` is the same commits on upstream main, ready for the next release
  branch. Check out whichever one you are changing.
