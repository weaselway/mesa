# 0007. EGL's software device reports the dxgdrm render node for d3d12

Status: Accepted

## Context

When d3d12 comes up through dxcore instead of the node (see 0002), EGL
treats it as its software device, and `EGL_EXT_device_drm_render_node`
returns no path for that by design. mutter gates both on the path: it drops
`zwp_linux_dmabuf_v1` to version 3, without feedback
(`meta-wayland-dma-buf.c:1932`), and does not advertise linux-drm-syncobj-v1
(`meta-wayland-linux-drm-syncobj.c:481`). The device query comes from a
client asking about its own display, which never calls `eglQueryDevicesEXT()`.

## Decision

- `_eglQueryDeviceStringEXT()` returns the path of the dxgdrm render node for
  the software device, if there is one.
- The node is found once, lazily, from the first query.
- Its driver is identified through `/sys/dev/char/M:m/device/driver`. Nodes of
  other GPUs are not opened.
- No node is reported when `/dev/dxg` is missing, `LIBGL_ALWAYS_SOFTWARE` is
  set, or `GALLIUM_DRIVER` names a driver other than d3d12.

## Consequences

- mutter offers linux-drm-syncobj-v1 and dma-buf feedback, which moves
  `zwp_linux_dmabuf_v1` from version 3 to 5.
- A forced llvmpipe stays a plain software device.
- The check uses environment variables, not the driver actually loaded. A
  software device that is not d3d12, on a machine with `/dev/dxg` and dxgdrm
  and no variable set, would report the node.
