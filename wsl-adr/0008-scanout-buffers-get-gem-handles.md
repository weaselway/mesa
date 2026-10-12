# 0008. Scanout buffers on the dxgdrm KMS node get GEM handles

Status: Accepted

## Context

A compositor running on the virtual KMS display (dxgdrm 0011) renders to a
`gbm_surface` and passes `gbm_bo_get_handle()` to `drmModeAddFB2()`. d3d12
had no handle to return. dxgdrm wraps a D3D12 shared handle in a GEM object
through PRIME import (dxgdrm 0012), and the presenter opens it on its own
device. A page flip carries no fence. KWin assumes a cursor plane without
`IN_FORMATS` only takes `DRM_FORMAT_MOD_LINEAR`, asks gbm for
`GBM_BO_USE_LINEAR`, and on failure draws the cursor into the frame instead.

## Decision

- `WINSYS_HANDLE_TYPE_KMS` is answered only when the screen's fd is the
  loader's fd on the primary node (`dxgdrm_is_kms`). The dup shares the
  caller's GEM handle namespace.
- The first request creates a shared handle, imports it with
  `drmPrimeFDToHandle()`, closes the fd and keeps the GEM handle on the
  resource. The handle is closed when the resource is destroyed.
- The handle reports `DRM_FORMAT_MOD_LINEAR`, the only modifier in the
  planes' `IN_FORMATS`. It first reported `DRM_FORMAT_MOD_INVALID`, on the
  assumption that the node had no plane for an explicit modifier. aquamarine
  (Hyprland) drops a cursor buffer with that modifier, so Hyprland showed no
  cursor.
- On that node only, `resource_create_with_modifiers` accepts
  `DRM_FORMAT_MOD_LINEAR` with the default layout. aquamarine allocates its
  cursor buffers with the cursor plane's modifiers. Elsewhere the hook stays
  unset, since its presence counts as support for modifiers in general.
- On that node, `PIPE_BIND_SCANOUT` with `PIPE_BIND_LINEAR` is accepted with
  the default layout. Only the presenter reads the buffer, through the shared
  handle.
- Writes to a buffer with a GEM handle are always waited out on flush, also
  for a client that exports sync_files (see 0004).

## Consequences

- Compositors run unmodified on the KMS node, with a hardware cursor plane in
  KWin and Hyprland.
- Compositors create their framebuffers with an explicit `LINEAR` modifier.
- The compositor's GPU work and its page flip are serialized by a CPU wait
  per frame.
- A buffer requested as linear on the KMS node is not linear. Nothing on the
  node can tell.
