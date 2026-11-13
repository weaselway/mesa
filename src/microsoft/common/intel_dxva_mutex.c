/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#include "intel_dxva_mutex.h"
#include "wsl_got.h"

#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "util/log.h"
#include "util/simple_mtx.h"

/* Intel's video driver for WSL (libigd12dxva64.so, seen with the HD 630
 * driver 31.0.101.2145) creates its mutexes with pthread_mutex_init(m, NULL)
 * and then locks some of them again on the same thread. That works with the
 * Windows critical sections the code was written for, but a default pthread
 * mutex deadlocks: the first DecodeFrame never returns.
 *
 * The driver library is loaded with the D3D12 device and calls
 * pthread_mutex_init through its GOT, so we replace that slot with a version
 * that makes default mutexes recursive. Mutexes it created while loading stay
 * as they are; the one that deadlocks belongs to the video decoder, which is
 * created later.
 */
#define DRIVER_NAME "libigd12dxva64.so"
#define MAX_COPIES 4

static simple_mtx_t lock = SIMPLE_MTX_INITIALIZER;

static int
recursive_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
   if (attr)
      return pthread_mutex_init(mutex, attr);

   pthread_mutexattr_t recursive;
   pthread_mutexattr_init(&recursive);
   pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
   int ret = pthread_mutex_init(mutex, &recursive);
   pthread_mutexattr_destroy(&recursive);
   return ret;
}

struct copies {
   char *paths[MAX_COPIES];
   unsigned count;
};

static int
find_driver(struct dl_phdr_info *info, size_t size, void *data)
{
   struct copies *copies = data;
   const char *base = strrchr(info->dlpi_name, '/');
   if (base && strcmp(base + 1, DRIVER_NAME) == 0 && copies->count < MAX_COPIES)
      copies->paths[copies->count++] = strdup(info->dlpi_name);
   return 0;
}

static void
patch(const char *path)
{
   /* Only takes a reference: the driver is already loaded. */
   void *handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
   if (!handle)
      return;

   struct link_map *lm;
   void **slot = NULL;
   if (dlinfo(handle, RTLD_DI_LINKMAP, &lm) == 0)
      slot = wsl_got_find_slot(lm, "pthread_mutex_init");
   if (!slot || *slot == (void *)recursive_mutex_init || !wsl_got_pin_self()) {
      dlclose(handle);
      return;
   }

   /* The driver is linked with BIND_NOW and RELRO, so the GOT is read-only. */
   bool relro = wsl_got_is_relro(lm, slot);
   long page_size = sysconf(_SC_PAGESIZE);
   void *page = (void *)((uintptr_t)slot & ~(uintptr_t)(page_size - 1));
   if (relro && mprotect(page, page_size, PROT_READ | PROT_WRITE) != 0) {
      mesa_logw("intel_dxva_mutex: GOT of %s is read-only, video may hang", path);
   } else {
      __atomic_store_n(slot, (void *)recursive_mutex_init, __ATOMIC_RELEASE);
      if (relro)
         mprotect(page, page_size, PROT_READ);
   }
   dlclose(handle);
}

void
intel_dxva_mutex_fix(void)
{
   simple_mtx_lock(&lock);
   /* dlopen must not run inside dl_iterate_phdr, it holds the loader lock. */
   struct copies copies = {0};
   dl_iterate_phdr(find_driver, &copies);
   for (unsigned i = 0; i < copies.count; i++) {
      patch(copies.paths[i]);
      free(copies.paths[i]);
   }
   simple_mtx_unlock(&lock);
}
