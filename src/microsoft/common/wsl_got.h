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

/* Keeps the Mesa library that contains this code loaded for good. A slot
 * patched to point into it would otherwise dangle once the Vulkan loader or
 * glvnd unloads it, and after a reload at the same address the old hook looks
 * like another copy's. Call it before patching a slot.
 */
bool wsl_got_pin_self(void);

#ifdef __cplusplus
}
#endif

#endif
