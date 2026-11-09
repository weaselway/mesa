/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef WSL_DEFAULT_ADAPTER_H
#define WSL_DEFAULT_ADAPTER_H

#include <directx/dxcore.h>
#include <stdlib.h>
#include <string.h>

#include "util/os_misc.h"

/* Index of the adapter d3d12 takes when nobody names one by LUID, in a
 * DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS list: the first whose description
 * contains MESA_D3D12_DEFAULT_ADAPTER_NAME (Linux only), else the first
 * integrated one, else the first. dzn uses it to know which adapter a d3d12
 * compositor runs on. -1 for an empty list.
 *
 * That is a guess: it is only right if the compositor and the client see the
 * same MESA_D3D12_DEFAULT_ADAPTER_NAME. A client started with another value
 * presents dma-bufs from an adapter the compositor cannot open them on.
 * Nothing tells a client the compositor's LUID, so it cannot check.
 */
static inline int
wsl_default_adapter_index(IDXCoreAdapterList *list)
{
   unsigned count = list->GetAdapterCount();
   IDXCoreAdapter *adapter;

#ifndef _WIN32
   const char *adapter_name = os_get_option("MESA_D3D12_DEFAULT_ADAPTER_NAME");
   if (adapter_name) {
      for (unsigned i = 0; i < count; i++) {
         if (FAILED(list->GetAdapter(i, &adapter)))
            continue;

         size_t desc_size;
         char *desc = NULL;
         if (SUCCEEDED(adapter->GetPropertySize(DXCoreAdapterProperty::DriverDescription, &desc_size)) &&
             (desc = (char *)malloc(desc_size)) &&
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::DriverDescription, desc_size, desc))) {
            free(desc);
            desc = NULL;
         }
         adapter->Release();

         bool match = desc && strcasestr(desc, adapter_name);
         free(desc);
         if (match)
            return i;
      }
   }
#endif

   for (unsigned i = 0; i < count; i++) {
      if (FAILED(list->GetAdapter(i, &adapter)))
         continue;
      bool is_integrated;
      bool ok = SUCCEEDED(adapter->GetProperty(DXCoreAdapterProperty::IsIntegrated, &is_integrated));
      adapter->Release();
      if (ok && is_integrated)
         return i;
   }

   return count ? 0 : -1;
}

#endif
