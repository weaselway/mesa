# 0001. The changes are one commit stack, on a release branch and on main

Status: Accepted

## Context

weaselway builds nixpkgs' Mesa with its `src` replaced (weaselway 0014), so
the source has to be the Mesa release nixpkgs packages, plus the WSL changes.
Upstream `main` moves on between releases and changes internal interfaces
the stack touches, such as the EGL loader callbacks and the DRI screen's
loader fields. The changes are also meant to go upstream one day, which needs
them on `main`.

## Decision

- Every change is a commit in this repository, not a patch file in weaselway.
- For each Mesa release that nixpkgs ships, a `mesa-X.Y.Z-wsl` branch carries
  the stack on the upstream tag. weaselway's `mesa-src` input follows that
  branch (weaselway 0015).
- `main` carries the same stack on upstream `main`, rebased when upstream
  moves. The next release branch is cut from it.
- A change goes onto the release branch weaselway builds and onto `main`. A
  commit on the release branch with no counterpart on `main` is a bug.
- The flake and `weaselway-build.sh` on `main` are a compile check only (see
  WEASELWAY.md). What ships is built by weaselway.
- A rebase rewrites the hashes, so these records name commits by subject.

## Consequences

- Each new release branch means rebasing the stack onto an older tree. Where
  upstream changed an interface in between, the two copies of a commit
  differ. `git range-diff` between the release branch and `main` shows which
  commits differ and how.
- Old release branches stay in place, so a locked weaselway release can still
  fetch its revision.
