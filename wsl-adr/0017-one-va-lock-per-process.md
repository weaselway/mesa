# 0017. All VADisplays of a process share one lock

Status: Accepted

## Context

Chrome opens a separate VADisplay for each codec, so a WebRTC call that
decodes and encodes H.264 in hardware has a decoder and an encoder on two
displays, used from two threads. The VA frontend locked per display, so the
two ran in parallel. Each display has its own d3d12 screen, but D3D12 hands
out one device per adapter, so both end up in the same vendor driver.

On an Intel Arc B390 the GPU process then crashed within seconds, in
`ExecuteCommandLists` of the Intel driver (`libigd12dxva64.so`), called from
either `d3d12_video_encoder_flush` or `d3d12_video_decoder_flush`. The
driver reads a 128 KB dxgkrnl mapping that was already unlocked and freed.
Encoding alone (decoding in software) ran without a crash, and so did
decoding and encoding with one lock for the whole process.

## Decision

- The frontend's per-display `drv->mutex` is replaced by one process-wide
  `vl_va_mutex`, initialized once.
- `vlVaTerminate` takes it while destroying the display's contexts and
  screen, because that releases objects of the shared device too.
- The lock is taken for every driver, not only d3d12. On WSL d3d12 is the
  only VA driver.
- Commits: "frontends/va: share one lock between all displays".

## Consequences

- VA calls of different displays in one process no longer overlap. The
  frontend already drops the lock while it waits for a fence
  (`vaSyncSurface`, `vaSyncBuffer`), so a decode does not wait for an encode
  to finish on the GPU, only for its submission.
- `vlVaInitialize` still creates its contexts without the lock.
- The underlying bug is in the vendor driver. Upstream would rather lock in
  d3d12 per device, which needs finding every call that races. Not
  upstreamable as it is.
