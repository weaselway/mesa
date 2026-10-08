# 0011. The opaque 10-bit format borrows the A2 DXGI format

Status: Accepted

## Context

EGL sorts 10-bit configs first. For an `EGL_EXT_present_opaque` surface Mesa
requires the compositor to advertise the opaque twin of the buffer format,
XB30 for AB30. d3d12 mapped no `R10G10B10X2` format, so no compositor using
d3d12 ever advertised XB30, and a client that picked a 10-bit config could
not create the surface.

## Decision

- `PIPE_FORMAT_R10G10B10X2_UNORM` maps to `DXGI_FORMAT_R10G10B10A2_UNORM`, as
  the X8 formats map to their A8 counterparts. DXGI has no X2 format.

## Consequences

- Opaque 10-bit surfaces work.
- The alpha bits of such a buffer hold whatever was written to them.
