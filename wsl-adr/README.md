# Decision records (ADR)

Each file records one decision of the weaselway Mesa fork: the context, the
decision and its consequences, including known risks. They were written after
the fact, from the commit messages, the comments in the changed code and
WEASELWAY.md. Fixes that involve no decision, such as the buffer reclaim
deadlock or the `drmGetDevices2()` count, are only in the commit history.

New decisions get the next number. A changed decision gets a new record, and
the old one gets the status "Superseded by NNNN". Records of the distribution
and of weaselwayd are in weaselway's `docs/adr`, and those of the kernel
module in dxgdrm's `docs/adr`. They are referred to as "weaselway NNNN" and
"dxgdrm NNNN". Commits are named by subject, because the stack is rebased
(see 0001).

| No. | Decision | Status |
|---|---|---|
| [0001](0001-patch-stack-on-release-branches-and-main.md) | The changes are one commit stack, on a release branch and on `main` | Accepted |
| [0002](0002-d3d12-as-swrast-target-with-dma-bufs.md) | Without a DRM node, d3d12 loads as a swrast target and still passes dma-bufs | Accepted |
| [0003](0003-shared-handles-as-dma-bufs.md) | A dma-buf from d3d12 is a D3D12 shared handle | Accepted |
| [0004](0004-exported-writes-waited-out-on-the-cpu.md) | Writes to a shared buffer are waited out on the CPU, unless the client syncs explicitly | Accepted |
| [0005](0005-fence-fds-are-sync-files-from-dxgdrm.md) | Native fence fds are sync_files made by dxgdrm, or eventfds without it | Accepted |
| [0006](0006-pipe-loader-claims-the-dxgdrm-node.md) | The pipe loader claims the dxgdrm node for d3d12 | Accepted |
| [0007](0007-software-device-reports-the-dxgdrm-node.md) | EGL's software device reports the dxgdrm render node for d3d12 | Accepted |
| [0008](0008-scanout-buffers-get-gem-handles.md) | Scanout buffers on the dxgdrm KMS node get GEM handles | Accepted |
| [0009](0009-async-readback-by-texture-to-buffer-copy.md) | `glReadPixels` into a PBO copies texture to buffer, asynchronously | Accepted |
| [0010](0010-wayland-swaps-throttle-on-frame-callbacks.md) | Wayland swaps always wait for the frame callback, for at most a second | Accepted |
| [0011](0011-opaque-10-bit-format.md) | The opaque 10-bit format borrows the A2 DXGI format | Accepted |

## Known risks

Found while writing these records, details in the linked files:

- Clients that rely on implicit sync wait for the GPU on the CPU for every
  frame they hand to the compositor, and so does the compositor for every
  frame it flips ([0004](0004-exported-writes-waited-out-on-the-cpu.md),
  [0008](0008-scanout-buffers-get-gem-handles.md)).
- A client that exports one sync_file is assumed to sync all its buffers
  explicitly, and can tear if it does not
  ([0004](0004-exported-writes-waited-out-on-the-cpu.md)).
- A d3d12 buffer handed to anything other than d3d12 cannot be used, because
  it is a shared handle, not a dma-buf
  ([0003](0003-shared-handles-as-dma-bufs.md)).
- Some changes affect every driver: the relaxed `sync_valid_fd()`
  ([0005](0005-fence-fds-are-sync-files-from-dxgdrm.md)) and the Wayland swap
  throttle ([0010](0010-wayland-swaps-throttle-on-frame-callbacks.md)). They
  cannot go upstream as they are.
- The software device's render node is chosen from environment variables,
  not from the driver actually loaded
  ([0007](0007-software-device-reports-the-dxgdrm-node.md)).
- Each release branch is a separate rebase, and its commits can differ from
  those on `main` ([0001](0001-patch-stack-on-release-branches-and-main.md)).
