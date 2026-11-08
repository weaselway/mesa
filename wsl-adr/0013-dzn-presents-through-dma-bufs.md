# 0013. dzn presents through dma-bufs from dxgdrm, synced through DRM syncobjs

Status: Accepted

## Context

dzn reported itself to Vulkan WSI as a software device, so every Wayland
frame went through `wl_shm`. Mutter uploads each shm frame through a
temporary D3D12 texture. A MAILBOX client that renders several hundred frames
a second kept the compositor's GPU queue busy enough that the Windows viewer
dropped to about 50 fps.

A D3D12 shared handle is not a real dma-buf (see 0003): it has no reservation
object, so it carries no implicit fences, and other drivers cannot use it.
dxgdrm 1.1 (dxgdrm 0021) wraps a shared handle in a GEM object and exports it
as a real dma-buf through PRIME, and `DXGDRM_DMABUF_TO_SHARED` gives an
importer the shared handle back. Syncobjs come from DRM core on the dxgdrm
node (dxgdrm 0008), and sync files from queue fences through eventfds
(dxgdrm 0007, see 0005).

## Decision

- On the adapter a d3d12 compositor uses by default
  (`MESA_D3D12_DEFAULT_ADAPTER_NAME`, else the first integrated adapter, else
  the first), and only with dxgdrm 1.1 or newer, dzn reports the dxgdrm node
  through `VK_EXT_physical_device_drm` and is not a software WSI device. dzn
  and d3d12 share these rules in `src/microsoft/common`. Other adapters keep
  shm.
- Memory can be exported as `DMA_BUF`. The dma-buf is made once at
  allocation from the memory's shared handle. Images for `DMA_BUF` export get
  the typeless DXGI format, because the d3d12 importer accepts only that or
  its own format for the fourcc.
- The d3d12 importer first asks dxgdrm for the shared handle behind an fd.
  Other fds fail the ioctl and are opened as before.
- On that adapter, semaphores and fences exported as `OPAQUE_FD` or
  `SYNC_FD` are DRM syncobjs, so WSI uses linux-drm-syncobj-v1 when the
  compositor offers it, and dma-buf implicit sync otherwise. Unexported
  semaphores and fences stay D3D12 fences.
- A signal on a syncobj gets a sync file from dxgdrm that signals when the
  queue fence reaches the submit's value. D3D12 cannot wait on a sync file,
  so a wait on a syncobj that is not signalled yet goes to one thread per
  queue. It waits on the CPU and then signals a D3D12 fence that the queue
  waits on. The submitting thread never blocks.
- The Vulkan runtime takes the first timeline sync type for the timeline
  mode, so dzn can list the syncobj type after its own.

## Consequences

- vkcube MAILBOX on the compositor's adapter no longer slows the viewer:
  median 60 fps and at least 58 fps per second, against a median of 51 fps
  and a minimum of 33 fps with shm (NVIDIA 940MX, compositor on the same
  adapter).
- X11 clients get DRI3 with syncobjs through Xwayland.
- `OPAQUE_FD` semaphores on that adapter are syncobj fds, not D3D12 shared
  handles. Interop that opens them as shared handles breaks there.
- Clients on another adapter still go through shm and its upload.
- The compositor must run a Mesa with the importer change, and client and
  compositor must agree on the default adapter. A compositor on another
  adapter cannot open the buffers, as with EGL today.
- Each wait on a syncobj that is not signalled at submit costs a CPU thread
  wakeup, and each signal an eventfd and a sync file.
- d3d12 GL clients do not attach fences to the dma-buf, so their CPU wait
  from 0004 stays.
- `DMA_BUF` is not advertised in the external memory queries; only WSI uses
  it so far.
