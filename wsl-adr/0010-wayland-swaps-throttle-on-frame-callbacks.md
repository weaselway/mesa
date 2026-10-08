# 0010. Wayland swaps always wait for the frame callback, for at most a second

Status: Accepted

## Context

With `SwapInterval` 0, Wayland EGL only round-trips a `wl_display_sync`, which
paces nothing. An unthrottled client makes the compositor composite, and
weaselway read back and send, at whatever rate it renders. A surface that is
not presented gets no frame callback at all, for example a subsurface whose
parent is not mapped yet. Firefox's GTK main thread waits on its renderer
inside a draw handler, while the renderer waits for a callback that only the
main thread can bring about by mapping the parent.

## Decision

- `eglSwapBuffers` requests a frame callback for every swap, also at
  interval 0. Interval 0 behaves as 1.
- The wait for the callback ends after one second. The callback is then
  dropped and the swap continues.

## Consequences

- Clients render no faster than the compositor presents.
- Firefox maps its windows.
- A surface that is never presented renders at one frame per second.
- Both changes apply to every driver, not only d3d12. Benchmarks that set
  interval 0 to measure throughput are capped at the display rate.
