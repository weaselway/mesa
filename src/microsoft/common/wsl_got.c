/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#include "wsl_got.h"

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

bool
wsl_got_pin_self(void)
{
   Dl_info self;
   return dladdr((void *)wsl_got_pin_self, &self) && self.dli_fname &&
          dlopen(self.dli_fname, RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE);
}

/* glibc stores absolute addresses in the dynamic section, other loaders keep
 * them relative to the load bias.
 */
static uintptr_t
dyn_ptr(const struct link_map *lm, uintptr_t ptr)
{
   return ptr < lm->l_addr ? lm->l_addr + ptr : ptr;
}

void **
wsl_got_find_slot(const struct link_map *lm, const char *name)
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

bool
wsl_got_is_relro(const struct link_map *lm, void **slot)
{
   struct relro relro = {.base = lm->l_addr};
   dl_iterate_phdr(find_relro, &relro);
   return (uintptr_t)slot >= relro.start && (uintptr_t)slot < relro.end;
}
