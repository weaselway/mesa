# Copyright © Weaselway contributors
# SPDX-License-Identifier: MIT

if host_machine.system() == 'linux'
  libd3dkmt_priority = static_library(
    'd3dkmt_priority',
    files('d3dkmt_priority.c', 'intel_dxva_mutex.c', 'wsl_dxgdrm.c', 'wsl_got.c', 'wsl_sandbox.c'),
    include_directories : [inc_include, inc_src],
    dependencies : [idep_mesautil, dep_dl, dep_libdrm],
    gnu_symbol_visibility : 'hidden',
  )
  idep_d3dkmt_priority = declare_dependency(
    link_with : libd3dkmt_priority,
    dependencies : dep_libdrm,
    include_directories : include_directories('.'),
  )
else
  idep_d3dkmt_priority = declare_dependency(include_directories : include_directories('.'))
endif
