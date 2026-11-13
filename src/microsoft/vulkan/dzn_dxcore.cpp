/*
 * Copyright © Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "dzn_physical_device_enum.h"
#include <directx/dxcore.h>
#include <dxguids/dxguids.h>
#include "wsl_default_adapter.h"
#include "wsl_sandbox.h"

#include "util/u_dl.h"
#include "util/log.h"

VkResult
dzn_enumerate_physical_devices_dxcore(struct vk_instance *instance)
{
   wsl_sandbox_init();
   util_dl_library *dxcore = util_dl_open(UTIL_DL_PREFIX "dxcore" UTIL_DL_EXT);
   if (!dxcore) {
      mesa_loge("Failed to load DXCore\n");
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   using PFNDXCoreCreateAdapterFactory = HRESULT (APIENTRY*)(REFIID, void **);
   PFNDXCoreCreateAdapterFactory create_func = (PFNDXCoreCreateAdapterFactory)util_dl_get_proc_address(dxcore, "DXCoreCreateAdapterFactory");
   if (!create_func) {
      mesa_loge("Failed to load DXCoreCreateAdapterFactory\n");
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   IDXCoreAdapterFactory *factory;
   if (FAILED(create_func(IID_PPV_ARGS(&factory)))) {
      mesa_loge("Failed to create DXCore adapter factory\n");
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   IDXCoreAdapterList *list;
   if (FAILED(factory->CreateAdapterList(1, &DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS, IID_PPV_ARGS(&list)))) {
      factory->Release();
      mesa_loge("Failed to create DXCore adapter list\n");
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   /* An adapter the driver cannot use (no D3D12 device at feature level
    * 11_0) is skipped, so it does not hide the others. Enumeration fails
    * only if no adapter could be added, or on another error. */
   uint32_t adapter_count = list->GetAdapterCount();
   int default_index = wsl_default_adapter_index(list);
   VkResult result = adapter_count ? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS;
   IDXCoreAdapter *adapter;
   for (uint32_t i = 0; i < adapter_count; ++i) {
      VkResult adapter_result = VK_ERROR_INITIALIZATION_FAILED;
      if (SUCCEEDED(list->GetAdapter(i, IID_PPV_ARGS(&adapter)))) {
         dzn_physical_device_desc desc = { 0 };
         DXCoreHardwareID hardware_id;
         bool is_hardware;
         if (FAILED(adapter->GetProperty(DXCoreAdapterProperty::HardwareID, &hardware_id)) ||
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::DedicatedAdapterMemory, &desc.dedicated_video_memory)) ||
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::SharedSystemMemory, &desc.shared_system_memory)) ||
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::DedicatedSystemMemory, &desc.dedicated_system_memory)) ||
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &desc.adapter_luid)) ||
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &is_hardware)) ||
             FAILED(adapter->GetProperty(DXCoreAdapterProperty::DriverDescription, sizeof(desc.description), desc.description))) {
            mesa_loge("Failed to retrieve DXCore adapter properties\n");
         } else {
            desc.vendor_id = hardware_id.vendorID;
            desc.device_id = hardware_id.deviceID;
            desc.subsys_id = hardware_id.subSysID;
            desc.revision = hardware_id.revision;
            desc.is_warp = !is_hardware;
            desc.is_default = (int)i == default_index;
            adapter_result = dzn_instance_add_physical_device(instance, adapter, &desc);
         }

         adapter->Release();
      }

      if (adapter_result == VK_SUCCESS) {
         result = VK_SUCCESS;
      } else if (adapter_result != VK_ERROR_INITIALIZATION_FAILED) {
         result = adapter_result;
         break;
      }
   }

   list->Release();
   factory->Release();
   return result;
}

void
dzn_query_memory_info(IUnknown* unk, d3d12_memory_info* output){
   IDXCoreAdapter* adapter = NULL;
   HRESULT hr = unk->QueryInterface(
      __uuidof(IDXCoreAdapter),
      reinterpret_cast<void**>(&adapter));

   if(SUCCEEDED(hr)){

      DXCoreAdapterMemoryBudget local_info, nonlocal_info;
      DXCoreAdapterMemoryBudgetNodeSegmentGroup local_node_segment = { 0, DXCoreSegmentGroup::Local };
      DXCoreAdapterMemoryBudgetNodeSegmentGroup nonlocal_node_segment = { 0, DXCoreSegmentGroup::NonLocal };
      adapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &local_node_segment, &local_info);
      adapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &nonlocal_node_segment, &nonlocal_info);

      output->budget_local = local_info.budget;
      output->budget_nonlocal = nonlocal_info.budget;
      output->budget = local_info.budget + nonlocal_info.budget;
      output->usage_local = local_info.currentUsage;
      output->usage_nonlocal = nonlocal_info.currentUsage;
      output->usage = local_info.currentUsage + nonlocal_info.currentUsage;
      return;
   }
}