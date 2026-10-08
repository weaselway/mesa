# 0002. Without a DRM node, d3d12 loads as a swrast target and still passes dma-bufs

Status: Accepted

## Context

Under WSL the GPU is reached through `/dev/dxg` and dxcore, not through a DRM
node. Mesa can only load d3d12 as a software (swrast) target, and a swrast
display in Wayland EGL presents through `wl_shm`: every frame is copied
through the CPU. `u_init_pipe_screen_caps()` learns the dma-buf caps from
`DRM_CAP_PRIME`, so without a node the screen has none, EGL hides
`EXT_image_dma_buf_import`, and the compositor gets shm buffers from every
client. The swrast loader in `sw_helper.h` also passed empty entries through
as the default driver, so llvmpipe shadowed d3d12 unless `GALLIUM_DRIVER`
was set.

With the dxgdrm module loaded the GPU has a DRM node (see 0006), and this
path is not taken. It remains the path without the module.

## Decision

- On Linux the d3d12 screen sets `caps->dmabuf` to import and export.
- Wayland EGL comes up as `swrast_dmabuf` when the compositor offers
  `zwp_linux_dmabuf_v1` but names no render node it can open. The driver is
  loaded as `swrast` (or zink), and the fd-based device probing is skipped.
- A swrast DRI screen whose loader supplies an image loader allocates its
  drawables from the driver, as a DRI2 screen does, instead of using shm.
- On such a display, flush and resize handling run as for a GPU display.
- eglInitialize fails this path when the driver cannot both import and export
  dma-bufs, so EGL retries with plain software rendering.
- `sw_screen_create_vk()` skips empty driver entries.

## Consequences

- Clients render on the GPU and hand the compositor buffers without a CPU
  copy, also without dxgdrm.
- The buffers are D3D12 shared handles, not dma-bufs (see 0003), and carry
  no fence (see 0004).
- A swrast driver that advertises dma-buf export now changes how Wayland EGL
  behaves for it. d3d12 is the only one that does.
