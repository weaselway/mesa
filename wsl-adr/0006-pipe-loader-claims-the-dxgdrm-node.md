# 0006. The pipe loader claims the dxgdrm node for d3d12

Status: Accepted

## Context

The dxgdrm module gives the GPU a DRM node that allocates no GPU memory
(dxgdrm 0001, 0002). Without a descriptor for its name, the pipe loader falls
back to kmsro, which fails, and EGL demotes the display to a software device.
Clients then see `EGL_MESA_device_software` and conclude there is no GPU.
Firefox reported `FEATURE_FAILURE_NO_DRM_DEVICE` and turned acceleration off.
The screen also needs an fd on the node for fences (see 0005) and scanout
imports (see 0008). Chromium's GPU sandbox closes after screen creation, so
the node cannot be opened later.

## Decision

- The pipe loader has a `dxgdrm` descriptor, before kmsro. It creates the
  d3d12 dxcore screen with no sw winsys. The GPU is still reached through
  `/dev/dxg`.
- The screen keeps a dup of the loader's fd
  (`d3d12_create_dxcore_screen_drm()`). A screen created another way scans
  the DRM devices for a render node whose driver is named `dxgdrm`, at screen
  creation. No node is not an error.
- `screen->get_fd()` returns that fd when there is no winsys.
- Without a winsys, `PIPE_BIND_DISPLAY_TARGET` format checks are left to
  D3D12. Asking the null winsys rejected every format and left no GL configs.

## Consequences

- With the module loaded, gbm and EGL on the node get d3d12, and clients see a
  hardware device with a DRM node.
- Matching by name keeps the scan off real GPUs on the same machine.
- An unpatched Mesa matches nothing on the node and falls back to software.
