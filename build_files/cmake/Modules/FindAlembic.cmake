# SPDX-FileCopyrightText: 2016 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Alembic library
# Find the native Alembic includes and libraries
# This module defines
#  ALEMBIC_INCLUDE_DIRS, where to find Alembic headers, Set when
#                        ALEMBIC_INCLUDE_DIR is found.
#  ALEMBIC_LIBRARIES, libraries to link against to use Alembic.
#  ALEMBIC_ROOT_DIR, The base directory to search for Alembic.
#                    This can also be an environment variable.
#  ALEMBIC_FOUND, If false, do not try to use Alembic.
#

# If `ALEMBIC_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED ALEMBIC_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{ALEMBIC_ROOT_DIR})
  SET(ALEMBIC_ROOT_DIR $ENV{ALEMBIC_ROOT_DIR})
ELSE()
  SET(ALEMBIC_ROOT_DIR "")
ENDIF()

SET(_alembic_SEARCH_DIRS
  ${ALEMBIC_ROOT_DIR}
  /opt/lib/alembic
)

FIND_PATH(ALEMBIC_INCLUDE_DIR
  NAMES
    Alembic/Abc/All.h
  HINTS
    ${_alembic_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(ALEMBIC_LIBRARY
  NAMES
    Alembic
  HINTS
    ${_alembic_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
)

# handle the QUIETLY and REQUIRED arguments and set ALEMBIC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Alembic DEFAULT_MSG ALEMBIC_LIBRARY ALEMBIC_INCLUDE_DIR)

IF(ALEMBIC_FOUND)
  SET(ALEMBIC_LIBRARIES ${ALEMBIC_LIBRARY})
  SET(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  ALEMBIC_INCLUDE_DIR
  ALEMBIC_LIBRARY
)

UNSET(_alembic_SEARCH_DIRS)
