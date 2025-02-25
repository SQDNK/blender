# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  importer
  ../common
  ../../blenkernel
  ../../bmesh
  ../../bmesh/intern
  ../../depsgraph
  ../../editors/include
  ../../makesrna
  ../../nodes
  ../../windowmanager
)

set(INC_SYS
  ../../../../extern/fast_float
)

set(SRC
  IO_stl.cc
  importer/stl_import.cc
  importer/stl_import_ascii_reader.cc
  importer/stl_import_binary_reader.cc
  importer/stl_import_mesh.cc

  IO_stl.h
  importer/stl_import.hh
  importer/stl_import_ascii_reader.hh
  importer/stl_import_binary_reader.hh
  importer/stl_import_mesh.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_io_common
)

blender_add_lib(bf_stl "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
