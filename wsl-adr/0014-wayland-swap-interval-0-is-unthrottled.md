# 0014. Wayland swaps at interval 0 do not wait for the frame callback

Status: Accepted

## Context

0010 made interval 0 behave as 1, so that an unthrottled client would not
make the compositor composite, and weaselway read back and send, at the
client's render rate. Mutter now composites at most once per vblank of
dxgdrm's virtual output (dxgdrm 0019), whatever rate a client renders at.
Benchmarks that set interval 0, such as glmark2, were still capped at 60 fps.

## Decision

- `eglSwapBuffers` requests a frame callback only when the swap interval is
  above 0, as upstream does.
- The one-second limit on the wait for a callback from 0010 stays.

## Consequences

- glmark2 on the Intel HD 630 went from 60 to about 540 fps, and the viewer
  stayed at 60 fps.
- An unthrottled client loads the GPU it shares with the compositor. Light
  clients did not slow the viewer; a heavy one may.
- The Wayland EGL code for interval 0 is upstream's again; only the timeout
  from 0010 differs.
