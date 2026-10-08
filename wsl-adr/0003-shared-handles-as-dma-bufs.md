# 0003. A dma-buf from d3d12 is a D3D12 shared handle

Status: Accepted

## Context

`WINSYS_HANDLE_TYPE_FD` maps to D3D12 `CreateSharedHandle` and
`OpenSharedHandle`. Under WSL the handle is a real fd and survives `SCM_RIGHTS`,
so it can travel wherever a dma-buf fd travels. It is not a dma-buf:
`OpenSharedHandle` recovers the layout from the resource, and stride, offset
and modifier mean nothing to it. D3D12 allows `ROW_MAJOR` only for buffers and
cross-adapter textures, so `CreateCommittedResource` rejects a 2D texture that
asks for it. `GBM_BO_USE_SCANOUT` (Chromium's `gfx::BufferUsage::SCANOUT`)
became exactly that request.

## Decision

- Exported and imported buffers are shared handles. The modifier is reported
  as `~0`, and the importer ignores stride and offset.
- `PIPE_BIND_SCANOUT` does not select a row-major layout on Linux. There is no
  display engine to read the buffer, and the importer does not see the tiling.
- `PIPE_BIND_LINEAR` keeps its meaning and still fails for 2D textures. A
  buffer that claims to be linear and is not would be worse.
- `PIPE_RESOURCE_PARAM_STRIDE` reports the natural row stride, not the
  256-byte aligned pitch of the staging layout. The padded value sheared every
  row at widths that are not a multiple of 64 pixels.

## Consequences

- Both ends of a hand-off have to be this driver. Any other importer gets an
  fd it cannot use.
- Chromium's scanout buffers allocate.
- `DMA_BUF_IOCTL_IMPORT_SYNC_FILE` and the other dma-buf ioctls fail on these
  fds, so implicit sync is not available (see 0004).
- On the dxgdrm KMS node, scanout buffers follow 0008 instead.
