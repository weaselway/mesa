# 0004. Writes to a shared buffer are waited out on the CPU, unless the client syncs explicitly

Status: Accepted

## Context

A dma-buf carries implicit fences, so a client may hand a buffer to the
compositor as soon as the frame is submitted. A D3D12 shared handle carries
none (see 0003), and the compositor runs on its own device and queue. Without
a wait it samples frames that are still being drawn. A static page commits
once, so the torn frame stays up until something else repaints.

Chromium does create a native fence. With no explicit-sync protocol it pushes
the fence into the buffer with `DMA_BUF_IOCTL_IMPORT_SYNC_FILE`, which fails
on a shared handle, and commits anyway. Chromium also rarely submits through
`pipe_context::flush`: one call against six exported buffers over 45 seconds.

## Decision

- A buffer is marked `exported` once its handle crosses a process boundary,
  in either direction. A batch lists the exported buffers it writes in
  `exported_writes`.
- `d3d12_flush_cmdlist()`, through which every submission passes, waits for
  such a batch's fence before returning.
- Wayland EGL on a `swrast_dmabuf` display (see 0002) also waits for the GPU
  in `eglSwapBuffers` before it attaches the buffer.
- The wait is skipped for buffers learned to be synced explicitly. A context
  collects the exported buffers it writes until it hands out a fence from
  `pipe_context::flush`, and the fence takes the list. If that fence is
  exported as a sync_file (see 0005), its buffers are marked `explicit_sync`
  for good. This is a client that hands its acquire fence to the compositor
  over linux-drm-syncobj-v1. A batch skips the wait only if every exported
  buffer it wrote is marked.
- Batches that only read exported buffers, as a compositor sampling its
  clients does, never wait.

## Consequences

- Clients that rely on implicit sync stall the CPU on the GPU for every frame
  that writes a shared buffer.
- The first frame on each buffer still waits.
- A buffer written by the same context between the same two fences as an
  explicitly synced one is marked too, and can tear if its consumer does not
  wait. Which buffer a fence goes with is only visible in the Wayland
  protocol traffic, which Mesa does not see.
- A buffer whose client stops passing fences stays marked.
- Scanout buffers on the KMS node are always waited out (see 0008).
