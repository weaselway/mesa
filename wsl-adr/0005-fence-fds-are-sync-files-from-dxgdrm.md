# 0005. Native fence fds are sync_files made by dxgdrm, or eventfds without it

Status: Accepted

## Context

Every d3d12 fence is backed by an eventfd that `SetEventOnCompletion()`
signals. Clients that end their frames with EGL fences instead of
`eglSwapBuffers` (Chromium's Ozone/Wayland backend) never trigger a flush
without `EGL_ANDROID_native_fence_sync`, and their rendering piles up
unsubmitted. An eventfd polls like a sync_file but fails `SYNC_IOC_FILE_INFO`,
and `drm_syncobj` will not import it. Chromium's explicit-sync path discards
the frame when that import fails (`wayland_surface.cc:444`). The WSL kernel is
built without `CONFIG_SW_SYNC`, so userspace cannot make a sync_file itself.

## Decision

- The d3d12 screen sets `native_fence_fd` on Linux, which turns on
  `EGL_ANDROID_native_fence_sync`.
- `fence_get_fd()` passes the fence's eventfd to
  `DRM_IOCTL_DXGDRM_FENCE_FROM_EVENTFD` and returns the sync_file (dxgdrm
  0007). Without the node, or if the ioctl fails, it returns a dup of the
  eventfd.
- An imported native fence fd becomes a CPU-only fence (`foreign_fd`).
  Waiting on it on a queue blocks the CPU, since there is no `ID3D12Fence` to
  wait on.
- `sync_valid_fd()` in `util/libsync.h` accepts any open fd, so that the
  eventfd fallback passes the debug check that aborts the process.
- `dxgdrm_drm.h` in `include/drm-uapi` is a copy of the module's header.

## Consequences

- With dxgdrm, explicit sync works end to end: Chromium, `drm_syncobj` and
  weaselwayd's readback fence (weaselway 0007).
- Without dxgdrm, fence fds still trigger submission, but cannot be imported,
  merged or queried.
- The relaxed `sync_valid_fd()` applies to every driver, not only d3d12.
- The header has to be synced by hand when the uapi changes. New fields are
  appended and must be zero (dxgdrm 0009).
