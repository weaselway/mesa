/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#include "wsl_dxgdrm.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>

#include "util/macros.h"

/* Matching on the driver name rather than taking the first node keeps this
 * from grabbing a real GPU on a machine that has one. */
int
wsl_open_dxgdrm_node(int min_minor)
{
   drmDevicePtr devices[8];
   /* drmGetDevices2() returns the total count, which can exceed what it
    * stored. */
   int num_devs = MIN2(drmGetDevices2(0, devices, ARRAY_SIZE(devices)),
                       (int)ARRAY_SIZE(devices));
   int fd = -1;

   for (int i = 0; i < num_devs && fd < 0; i++) {
      if (!(devices[i]->available_nodes & (1 << DRM_NODE_RENDER)))
         continue;

      int try_fd = open(devices[i]->nodes[DRM_NODE_RENDER],
                        O_RDWR | O_CLOEXEC);
      if (try_fd < 0)
         continue;

      drmVersionPtr version = drmGetVersion(try_fd);
      if (version) {
         if (!strcmp(version->name, "dxgdrm") &&
             (version->version_major > 1 ||
              (version->version_major == 1 && version->version_minor >= min_minor)))
            fd = try_fd;
         drmFreeVersion(version);
      }
      if (fd != try_fd)
         close(try_fd);
   }

   if (num_devs > 0)
      drmFreeDevices(devices, num_devs);

   return fd;
}
