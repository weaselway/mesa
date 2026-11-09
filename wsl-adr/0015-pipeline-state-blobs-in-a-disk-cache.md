# 0015. d3d12 keeps pipeline state blobs in a disk cache, except on NVIDIA

Status: Accepted

## Context

On WSL, the first draw with a new GL program spends 75-90% of its time in
the Windows driver, compiling DXIL to GPU code while it creates the pipeline
state object (PSO). d3d12 cached PSOs per context only, and neither driver on
the test machine caches across processes: both report no
`D3D12_SHADER_CACHE_SUPPORT_AUTOMATIC_*` cap, and NVIDIA's
`DRIVER_MANAGED_CACHE` did not make a second run faster. So every start of
every program paid the full cost again. On Intel HD 630 a PSO-only state
change costs 15-18 ms, and a large shader more than a second.

D3D12 lets an application keep this work itself:
`ID3D12PipelineState::GetCachedBlob` returns a blob that a later
`CreatePipelineState` takes as `CachedPSO`. A blob from another adapter,
driver or runtime version is rejected (`D3D12_ERROR_DRIVER_VERSION_MISMATCH`,
`D3D12_ERROR_ADAPTER_NOT_FOUND`).

## Decision

- The d3d12 screen opens a Mesa disk cache, `d3d12_pso`, whose identity is
  the build id plus the adapter's vendor id, device id and driver version.
  It follows the usual `MESA_SHADER_CACHE_*` variables.
- Graphics and compute PSOs are looked up there before they are created. The
  key hashes what the PSO description points to: the shader bytecode, the
  root signature's `d3d12_root_signature_key` (attached to each
  `ID3D12RootSignature` as private data), stream output, input layout and the
  fixed-function state. On a miss the blob from `GetCachedBlob` is stored.
- A creation that fails with a blob is retried without it, before the
  fail-fast `_Exit` that upstream has for failed graphics PSOs.
- Off by default on NVIDIA: driver 32.0.15.8266 aborts with a glibc double
  free in `GetCachedBlob` (and in `ID3D12PipelineLibrary::StorePipeline`),
  for graphics and compute PSOs. `MESA_D3D12_PSO_CACHE=0/1` overrides the
  default for any vendor.
- No pipeline library: one blob per PSO in the disk cache needs no file of
  our own and no locking between processes, and on Intel it was as fast.
- Commits: "d3d12: open a disk cache for pipeline state blobs",
  "d3d12: cache graphics pipeline state blobs on disk",
  "d3d12: cache compute pipeline state blobs on disk".

## Consequences

- On Intel HD 630 (driver 31.0.101.2145), first draws with a warm cache:
  fixed-function variants 15-17 ms to 2.5-3 ms, a 64-block lighting shader
  605 to 37 ms, 256 blocks 1378 to 430 ms. Intel still spends time loading
  large blobs (245 ms for 159 KB).
- gnome-shell creates 17 PSOs at start-up and finds all of them in a warm
  cache, but its start-up time does not change measurably: PSOs are a small
  part of it.
- NVIDIA gets nothing until its driver stops crashing. The opt-in exists to
  test new drivers.
- The compute path is untested: Intel exposes no compute shaders through
  d3d12 on the test machine, WSL has no WARP adapter, and NVIDIA crashes.
- A field missing from the key gives the driver a blob for another
  description. The D3D12 documentation only says that creation "might"
  then fail with `E_INVALIDARG` (which the retry handles), so a gap in the
  key can mean a wrong PSO. New PSO state must go into the key.
- The GLSL-to-DXIL compile is not cached; d3d12 still has no
  `get_disk_shader_cache`.
