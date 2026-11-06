/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef WSL_GOT_H
#define WSL_GOT_H

#include <link.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GOT slot through which the library calls the function name, or NULL. */
void **wsl_got_find_slot(const struct link_map *lm, const char *name);

/* Whether the slot lies in the library's RELRO segment, which the loader
 * made read-only after relocation.
 */
bool wsl_got_is_relro(const struct link_map *lm, void **slot);

#ifdef __cplusplus
}
#endif

#endif
