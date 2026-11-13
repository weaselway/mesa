/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#include "wsl_sandbox.h"
#include "wsl_got.h"

#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util/log.h"
#include "util/u_call_once.h"

/* In a Flatpak sandbox /usr belongs to the runtime and /dev only has the
 * devices the app may use. weaselway's GL extension mounts WSL's libraries
 * and driver store at wsl/lib and wsl/drivers next to our lib/ directory,
 * and the dri device permission brings /dev/dri/dxg, a copy of the /dev/dxg
 * node. libdxcore.so opens /dev/dxg while it loads, too early for a hook,
 * but the sandbox's /dev is a tmpfs the app owns, so we add a link to the
 * copy there. libd3d12core.so loads the vendor driver from the /usr/lib/wsl/drivers path
 * the host reports, which we rewrite through its GOT slot for dlopen.
 */
#define WSL_PREFIX "/usr/lib/wsl/"
#define DXG "/dev/dxg"
#define SANDBOX_DXG "/dev/dri/dxg"

static char root[PATH_MAX];

/* shortcut: the loader now sees us as the caller, so names without a slash
 * no longer search libd3d12core.so's RUNPATH ($ORIGIN). The WSL libraries
 * it loads by name are loaded already; others would need that search.
 */
static void *
hook_dlopen(const char *path, int flags)
{
   char redirected[PATH_MAX];
   if (path && strncmp(path, WSL_PREFIX, strlen(WSL_PREFIX)) == 0 &&
       snprintf(redirected, sizeof(redirected), "%s/%s", root,
                path + strlen(WSL_PREFIX)) < (int)sizeof(redirected))
      path = redirected;
   return dlopen(path, flags);
}

static bool
patch(const char *library, const char *name, void *hook)
{
   /* Never closed: the hook must stay as long as the library is loaded. */
   void *handle = dlopen(library, RTLD_LAZY | RTLD_LOCAL);
   struct link_map *lm;
   if (!handle || dlinfo(handle, RTLD_DI_LINKMAP, &lm) != 0)
      return false;

   void **slot = wsl_got_find_slot(lm, name);
   if (!slot || wsl_got_is_relro(lm, slot))
      return false;

   __atomic_store_n(slot, hook, __ATOMIC_RELEASE);
   return true;
}

static void
install(void)
{
   Dl_info info;
   char lib[PATH_MAX];
   if (access(DXG, F_OK) == 0 || !dladdr((void *)wsl_sandbox_init, &info) ||
       !info.dli_fname || snprintf(lib, sizeof(lib), "%s", info.dli_fname) >= (int)sizeof(lib))
      return;

   /* .../GL/host/lib/libgallium.so -> .../GL/host/wsl */
   snprintf(root, sizeof(root), "%s/wsl", dirname(dirname(lib)));
   if (access(root, F_OK) != 0)
      return;

   /* WSL's libraries are not on the sandbox's library path, so we load them
    * by path, and later loads by name find these copies. libd3d12core.so
    * brings libdxcore.so, which opens /dev/dxg as it loads.
    */
   char core[PATH_MAX], d3d12[PATH_MAX];
   snprintf(core, sizeof(core), "%s/lib/libd3d12core.so", root);
   snprintf(d3d12, sizeof(d3d12), "%s/lib/libd3d12.so", root);
   if ((access(SANDBOX_DXG, F_OK) == 0 && symlink("dri/dxg", DXG) != 0) ||
       !patch(core, "dlopen", (void *)hook_dlopen) || !dlopen(d3d12, RTLD_LAZY | RTLD_LOCAL))
      mesa_logw("wsl: cannot redirect WSL's paths into the sandbox, D3D12 will fail");
}

void
wsl_sandbox_init(void)
{
   static util_once_flag once = UTIL_ONCE_FLAG_INIT;
   util_call_once(&once, install);
}
