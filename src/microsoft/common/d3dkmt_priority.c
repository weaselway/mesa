/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#include "d3dkmt_priority.h"

#include <dlfcn.h>
#include <link.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>

#include "drm-uapi/d3dkmthk.h"
#include "util/log.h"
#include "util/simple_mtx.h"
#include "util/u_call_once.h"
#include "util/u_debug.h"

/* libd3d12core.so calls these libdxcore.so functions through its PLT. We
 * replace their GOT slots, so we see every context D3D12 creates and can set
 * its priority right after creation, on the same thread.
 */
typedef int (*create_context_fn)(struct d3dkmt_createcontextvirtual *args);
typedef int (*destroy_context_fn)(const struct d3dkmt_destroycontext *args);
typedef int (*set_priority_fn)(const struct d3dkmt_setcontextschedulingpriority *args);

struct context {
   uint32_t handle;
   bool pinned; /* created with a pending priority */
};

static create_context_fn real_create;
static destroy_context_fn real_destroy;
static set_priority_fn set_priority;
static bool debug;

static simple_mtx_t lock = SIMPLE_MTX_INITIALIZER;
static struct context *contexts;
static unsigned context_count, context_cap;
static int requested = D3DKMT_PRIORITY_NONE;

static __thread int pending = D3DKMT_PRIORITY_NONE;

static int
process_priority(void)
{
   return requested == D3DKMT_PRIORITY_NONE ? D3DKMT_PRIORITY_MEDIUM : requested;
}

static void
apply(uint32_t handle, int priority)
{
   struct d3dkmt_setcontextschedulingpriority args = {
      .context.v = handle,
      .priority = priority,
   };
   int status = set_priority(&args);
   if (status != 0)
      mesa_logw("d3dkmt: setting priority %d on context 0x%x failed: 0x%x",
                priority, handle, status);
   else if (debug)
      mesa_logi("d3dkmt: context 0x%x priority %d", handle, priority);
}

static int
hook_create_context(struct d3dkmt_createcontextvirtual *args)
{
   int status = real_create(args);
   if (status != 0)
      return status;

   simple_mtx_lock(&lock);
   if (context_count == context_cap) {
      unsigned cap = context_cap ? context_cap * 2 : 16;
      struct context *grown = realloc(contexts, cap * sizeof(*contexts));
      if (!grown) {
         simple_mtx_unlock(&lock);
         return status;
      }
      contexts = grown;
      context_cap = cap;
   }

   bool pinned = pending != D3DKMT_PRIORITY_NONE;
   contexts[context_count++] = (struct context) {args->context.v, pinned};
   apply(args->context.v, pinned ? pending : process_priority());
   simple_mtx_unlock(&lock);
   return status;
}

static int
hook_destroy_context(const struct d3dkmt_destroycontext *args)
{
   simple_mtx_lock(&lock);
   for (unsigned i = 0; i < context_count; i++) {
      if (contexts[i].handle == args->context.v) {
         contexts[i] = contexts[--context_count];
         break;
      }
   }
   simple_mtx_unlock(&lock);
   return real_destroy(args);
}

/* glibc stores absolute addresses in the dynamic section, other loaders keep
 * them relative to the load bias.
 */
static uintptr_t
dyn_ptr(const struct link_map *lm, uintptr_t ptr)
{
   return ptr < lm->l_addr ? lm->l_addr + ptr : ptr;
}

static void **
find_got_slot(const struct link_map *lm, const char *name)
{
   const ElfW(Rela) *rel = NULL;
   const ElfW(Sym) *symtab = NULL;
   const char *strtab = NULL;
   size_t rel_size = 0;
   ElfW(Sxword) rel_type = 0;

   for (const ElfW(Dyn) *dyn = lm->l_ld; dyn->d_tag != DT_NULL; dyn++) {
      switch (dyn->d_tag) {
      case DT_JMPREL: rel = (const void *)dyn_ptr(lm, dyn->d_un.d_ptr); break;
      case DT_PLTRELSZ: rel_size = dyn->d_un.d_val; break;
      case DT_PLTREL: rel_type = dyn->d_un.d_val; break;
      case DT_SYMTAB: symtab = (const void *)dyn_ptr(lm, dyn->d_un.d_ptr); break;
      case DT_STRTAB: strtab = (const void *)dyn_ptr(lm, dyn->d_un.d_ptr); break;
      }
   }
   if (!rel || !symtab || !strtab || rel_type != DT_RELA)
      return NULL;

   for (size_t i = 0; i < rel_size / sizeof(*rel); i++) {
#if UINTPTR_MAX > 0xffffffff
      const ElfW(Sym) *sym = &symtab[ELF64_R_SYM(rel[i].r_info)];
#else
      const ElfW(Sym) *sym = &symtab[ELF32_R_SYM(rel[i].r_info)];
#endif
      if (strcmp(strtab + sym->st_name, name) == 0)
         return (void **)(lm->l_addr + rel[i].r_offset);
   }
   return NULL;
}

struct relro {
   ElfW(Addr) base;
   uintptr_t start, end;
};

static int
find_relro(struct dl_phdr_info *info, size_t size, void *data)
{
   struct relro *relro = data;
   if (info->dlpi_addr != relro->base)
      return 0;
   for (unsigned i = 0; i < info->dlpi_phnum; i++) {
      if (info->dlpi_phdr[i].p_type == PT_GNU_RELRO) {
         relro->start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
         relro->end = relro->start + info->dlpi_phdr[i].p_memsz;
      }
   }
   return 1;
}

/* A slot still pointing into libd3d12core.so is the lazy binding stub. Any
 * other value is either the resolved function or the hook of another Mesa
 * driver in this process, which we chain to.
 */
static void *
resolve(void *core, const struct link_map *lm, void *current, const char *name)
{
   Dl_info info;
   if (dladdr(current, &info) && info.dli_fname && strcmp(info.dli_fname, lm->l_name) != 0)
      return current;
   return dlsym(core, name);
}

static void
install(void)
{
   debug = debug_get_bool_option("MESA_D3DKMT_PRIORITY_DEBUG", false);

   /* libd3d12.so loads libd3d12core.so by this name when the first device is
    * created and gets this copy then. It is never closed, the hook must stay.
    */
   void *core = dlopen("libd3d12core.so", RTLD_LAZY | RTLD_LOCAL);
   if (!core)
      return;

   struct link_map *lm;
   if (dlinfo(core, RTLD_DI_LINKMAP, &lm) != 0)
      return;

   void **create_slot = find_got_slot(lm, "D3DKMTCreateContextVirtual");
   void **destroy_slot = find_got_slot(lm, "D3DKMTDestroyContext");
   if (!create_slot || !destroy_slot) {
      mesa_logw("d3dkmt: context functions not found in libd3d12core.so, priorities disabled");
      return;
   }

   struct relro relro = {.base = lm->l_addr};
   dl_iterate_phdr(find_relro, &relro);
   if (((uintptr_t)create_slot >= relro.start && (uintptr_t)create_slot < relro.end) ||
       ((uintptr_t)destroy_slot >= relro.start && (uintptr_t)destroy_slot < relro.end)) {
      mesa_logw("d3dkmt: GOT of libd3d12core.so is read-only, priorities disabled");
      return;
   }

   set_priority = (set_priority_fn)dlsym(core, "D3DKMTSetContextSchedulingPriority");
   real_create = (create_context_fn)resolve(core, lm, *create_slot, "D3DKMTCreateContextVirtual");
   real_destroy = (destroy_context_fn)resolve(core, lm, *destroy_slot, "D3DKMTDestroyContext");
   if (!set_priority || !real_create || !real_destroy) {
      mesa_logw("d3dkmt: context functions not found in libdxcore.so, priorities disabled");
      return;
   }

   __atomic_store_n(destroy_slot, (void *)hook_destroy_context, __ATOMIC_RELEASE);
   __atomic_store_n(create_slot, (void *)hook_create_context, __ATOMIC_RELEASE);
}

void
d3dkmt_priority_init(void)
{
   static util_once_flag once = UTIL_ONCE_FLAG_INIT;
   util_call_once(&once, install);
}

void
d3dkmt_priority_request(int priority)
{
   simple_mtx_lock(&lock);
   int before = process_priority();
   if (requested == D3DKMT_PRIORITY_NONE || priority > requested)
      requested = priority;

   if (process_priority() != before && set_priority) {
      for (unsigned i = 0; i < context_count; i++) {
         if (!contexts[i].pinned)
            apply(contexts[i].handle, process_priority());
      }
   }
   simple_mtx_unlock(&lock);
}

void
d3dkmt_priority_set_pending(int priority)
{
   pending = priority;
}
