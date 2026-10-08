# 0009. glReadPixels into a PBO copies texture to buffer, asynchronously

Status: Accepted

## Context

weaselwayd reads every frame back with `glReadPixels` into a pixel buffer
object (weaselway 0007). `st_ReadPixels()` had only one asynchronous route
into a PBO, a fragment shader writing the buffer, which needs shader images.
Without them it mapped a staging texture: a wait for the GPU and a memcpy on
the calling thread, every frame. D3D12 can copy a texture region straight
into a buffer with `CopyTextureRegion()` and a `PLACED_FOOTPRINT`, but the
row pitch must be a multiple of 256 bytes and the placement offset a
multiple of 512.

## Decision

- A new pipe cap, `texture_to_buffer_copy_row_alignment`, says that
  `resource_copy_region()` takes a texture source and a buffer destination,
  with `dst_box->x` as a byte offset, when the row stride is a multiple of the
  value. 0, the default, means no such copies.
- `st_ReadPixels()` blits the region into a staging texture and copies that
  into the PBO, when the cap allows it. The staging texture is kept across
  calls.
- d3d12 sets the cap to 256. A destination offset that is not a multiple of
  512 goes through an aligned temporary buffer and `CopyBufferRegion()`.
- The written range is added to the destination's valid range, in bytes, in
  d3d12 and in the threaded context. Counting pixels let later maps of the
  PBO skip synchronization while the copy was in flight.

## Consequences

- Readback no longer stalls the calling thread. The fence of the EGL sync
  that follows the read signals when the copy is done.
- Callers choose widths whose row stride is a multiple of 256 bytes, 64
  pixels at 4 bytes per pixel. Other widths, and packed or offset layouts,
  fall back to the existing paths, on d3d12 the synchronous one.
- The cap is new gallium interface, set only by d3d12.
