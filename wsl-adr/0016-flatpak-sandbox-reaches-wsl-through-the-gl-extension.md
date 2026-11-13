# 0016. In a Flatpak sandbox, Mesa reaches WSL through its GL extension

Status: Accepted

## Context

weaselway's Ubuntu install script can build this Mesa as the local Flatpak GL
extension `org.freedesktop.Platform.GL.host`, so Flatpak apps get d3d12 and
dzn. In the sandbox, `/usr` belongs to the runtime and `/dev` only has the
devices the app may use. `libdxcore.so` opens `/dev/dxg` in a constructor,
and `libd3d12core.so` loads the vendor driver from the
`/usr/lib/wsl/drivers` path the host reports. Neither exists in the sandbox,
and neither library has an option to look elsewhere.

## Decision

- Outside Mesa, weaselway's session preparation makes a copy of the
  `/dev/dxg` node at `/dev/dri/dxg`, which the `dri` device permission brings
  into the sandbox, and binds `/usr/lib/wsl/lib` and `/usr/lib/wsl/drivers`
  into the extension's `wsl/` directory, next to its `lib/`.
- `wsl_sandbox_init()` runs before d3d12 or dzn load `libd3d12.so` or
  `libdxcore.so`. If the `wsl/` directory exists next to the directory of
  the loaded Mesa library, Mesa is in the extension:
  - it links `/dev/dxg` to `dri/dxg` in the sandbox's `/dev`, a tmpfs the
    app owns, unless the link is there already;
  - it loads `libd3d12core.so` and `libd3d12.so` from `wsl/lib` by path, so
    later loads by name find them;
  - it replaces `libd3d12core.so`'s GOT slot for `dlopen` with a hook that
    rewrites paths under `/usr/lib/wsl/` to the `wsl/` directory, with the
    GOT helper of 0012.
- The check is the `wsl/` directory, not a missing `/dev/dxg`: the second
  GPU process in a sandbox finds the link the first one made.
- Outside the extension, nothing changes.
- Commits: "microsoft/common: reach WSL's files from a Flatpak sandbox",
  "microsoft/common: redirect in every process of a sandbox".

## Consequences

- Flathub's Chromium and Firefox render on the GPU through d3d12, and dzn
  works for Flatpak apps, on the runtime branches the extension is built for.
- The hook makes Mesa the caller of `dlopen`, so names without a slash no
  longer search `libd3d12core.so`'s `$ORIGIN`. The WSL libraries it loads by
  name are loaded already; a new dependency would fail to load.
- Like 0012, it relies on the GOT of `libd3d12core.so` and stops working if a
  WSL update binds `dlopen` differently. Mesa then logs a warning and D3D12
  fails in the sandbox.
- The link to `/dev/dri/dxg` and the binds depend on weaselway's session
  preparation; without them Mesa falls back to softpipe in the sandbox.
