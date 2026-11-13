# 0012. GPU priorities are set on dxgkrnl contexts, through a GOT hook

Status: Accepted

## Context

D3D12 queue priorities have no effect on WSL. The priority of the dxgkrnl
context (the kernel object the GPU scheduler sees) does: lowering the
contexts of a loading process to -3 or below brings the latency of other
processes' small submissions close to idle. The measurements are in
weaselway 0033. D3D12 has no way to pass this priority, and the context
handles never leave `libd3d12core.so`, which creates them through
`D3DKMTCreateContextVirtual` in `libdxcore.so`.

## Decision

- `src/microsoft/common/d3dkmt_priority.c` opens `libd3d12core.so` before
  the first device is created and replaces its GOT (global offset table,
  through which it calls other libraries) entries of
  `D3DKMTCreateContextVirtual` and `D3DKMTDestroyContext`. The hook sets the
  priority of each new context right after it is created.
- Contexts start at -3. High priority is 0, low is -7. Realtime is not
  offered.
- d3d12 exposes low, medium and high through `context_priority_mask`, which
  turns on `EGL_IMG_context_priority`. All contexts of a screen share one
  queue, so the highest priority any context asked for applies to the whole
  process.
- dzn exposes `VK_KHR_global_priority`, `VK_EXT_global_priority` and
  `VK_EXT_global_priority_query`. Each D3D12 queue creates exactly one
  context on the calling thread, so the priority is passed to the hook in a
  thread-local variable while `CreateCommandQueue` runs.
- If a second Mesa driver in the process already replaced an entry, the
  hook calls that one. It then keeps the priority the other copy set, unless
  its own driver pinned the context or asked for a priority.
- Commits: "d3d12, dzn: set dxgkrnl context priorities on WSL".

## Consequences

- The hook depends on how `libd3d12core.so` is linked: lazy binding with a
  writable GOT, and loaded by the plain name. If a WSL update changes this,
  Mesa logs a warning and all contexts stay at 0.
- Once a hook is in, the Mesa library that holds it stays loaded until the
  process exits (`wsl_got_pin_self()`), so a patched slot never points into
  an unloaded or reloaded copy. The same goes for the hooks of 0016.
- The vendor driver creates further contexts through `libdxcore.so`
  directly. The hook does not see them; they stay at 0.
- A priority only helps as far as the GPU preempts. On a GPU that preempts
  only between DMA buffers (NVIDIA 940MX), a heavy client halves a light
  client's frame rate at any priority (weaselway 0033).
- Not upstreamable as it is: patching another library's GOT is a WSL
  workaround for a missing D3D12 interface.
- `MESA_D3DKMT_PRIORITY_DEBUG=1` with `MESA_LOG_LEVEL=info` logs every
  context and its priority.
