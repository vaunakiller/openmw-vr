# Locate OpenXR library
# This module defines
# OpenXR_LIBRARY, the OpenXR library, with no other libraries
# OpenXR_LIBRARIES, the OpenXR library and required components with compiler flags
# OpenXR_FOUND, if false, do not try to link to OpenXR
# OpenXR_INCLUDE_DIR, where to find openxr.h
# OpenXR_VERSION, the version of the found library
#
# This module accepts the following env variables
#  OPENXR_ROOT 
# This module responds to the the flag:

#=============================================================================
# Copyright 2003-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)


if(WIN32)
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_openxr_default_folder "C:/Program Files/OPENXR")
  else()
    set(_openxr_default_folder "C:/Program Files (x86)/OPENXR")
  endif()
endif(WIN32)

libfind_pkg_detect(OpenXR openxr
    FIND_PATH openxr/openxr.h
        HINTS $ENV{OPENXR_ROOT} ${_openxr_default_folder}
        PATH_SUFFIXES include
    FIND_LIBRARY openxr_loader.lib
        HINTS $ENV{OPENXR_ROOT} ${_openxr_default_folder}
        PATH_SUFFIXES lib lib32 lib64
)
libfind_version_n_header(OpenXR NAMES openxr/openxr.h DEFINES XR_VERSION_MAJOR XR_VERSION_MINOR XR_VERSION_PATCH)

libfind_process(OpenXR)
