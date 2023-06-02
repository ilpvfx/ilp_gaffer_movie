# Author: Tommy Hinks, tommy.hinks@ilpvfx.com
#
#[=======================================================================[.rst:

FindGaffer
-----------

Find Gaffer include dirs and libraries

Use this module by invoking find_package with the form::

  find_package(Gaffer
    [version] [EXACT]      # Minimum or EXACT version
    [REQUIRED]             # Fail with error if Gaffer is not found
    [COMPONENTS <libs>...] # Gaffer libraries by their canonical name
                           # e.g. "GafferScene" for "libGafferScene"
    )

IMPORTED Targets
^^^^^^^^^^^^^^^^

``Gaffer::Gaffer``
``Gaffer::GafferBindings``
``Gaffer::GafferScene``
``Gaffer::IECore``
``Gaffer::IECorePython``
``Gaffer::boost_python``
``Gaffer::python``

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Gaffer_FOUND``
  True if the system has the Gaffer library.
``Gaffer_VERSION``
  The version of the Gaffer library which was found.
``Gaffer_INCLUDE_DIRS``
  Include directories needed to use Gaffer.
``Gaffer_LIBRARIES``
  Libraries needed to link to Gaffer.
``Gaffer_LIBRARY_DIRS``
  Gaffer library directories.
``Gaffer_{COMPONENT}_FOUND``
  True if the system has the named Gaffer component.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Gaffer_INCLUDE_DIR``
  The directory containing ``Gaffer/Version.h``.
``Gaffer_{COMPONENT}_LIBRARY``
  Individual component libraries for Gaffer

Hints
^^^^^

Instead of explicitly setting the cache variables, the following variables
may be provided to tell this module where to look.

``GAFFER_ROOT``
  Preferred installation prefix.
``GAFFER_INCLUDEDIR``
  Preferred include directory e.g. <prefix>/include
``GAFFER_LIBRARYDIR``
  Preferred library directory e.g. <prefix>/lib

#]=======================================================================]

# Support new if() IN_LIST operator
if(POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

mark_as_advanced(
  Gaffer_INCLUDE_DIR
  Gaffer_LIBRARY
)

set(_GAFFER_COMPONENT_LIST
  Gaffer
  GafferBindings
  GafferScene
  IECore
  IECorePython
  boost_python
  python)

if(Gaffer_FIND_COMPONENTS)
  set(GAFFER_COMPONENTS_PROVIDED TRUE)
  set(_IGNORED_COMPONENTS "")
  foreach(COMPONENT ${Gaffer_FIND_COMPONENTS})
    if(NOT ${COMPONENT} IN_LIST _GAFFER_COMPONENT_LIST)
      list(APPEND _IGNORED_COMPONENTS ${COMPONENT})
    endif()
  endforeach()

  if(_IGNORED_COMPONENTS)
    message(STATUS "Ignoring unknown components of Gaffer:")
    foreach(COMPONENT ${_IGNORED_COMPONENTS})
      message(STATUS "  ${COMPONENT}")
    endforeach()
    list(REMOVE_ITEM Gaffer_FIND_COMPONENTS ${_IGNORED_COMPONENTS})
  endif()
else()
  # Try to find all components.
  set(GAFFER_COMPONENTS_PROVIDED FALSE)
  set(Gaffer_FIND_COMPONENTS ${_ILMBASE_COMPONENT_LIST})
endif()

# Append GAFFER_ROOT or $ENV{GAFFER_ROOT} if set (prioritize the direct cmake var).
set(_GAFFER_ROOT_SEARCH_DIR "")
if(GAFFER_ROOT)
  list(APPEND _GAFFER_ROOT_SEARCH_DIR ${GAFFER_ROOT})
else()
  set(_ENV_GAFFER_ROOT $ENV{GAFFER_ROOT})
  if(_ENV_GAFFER_ROOT)
    list(APPEND _GAFFER_ROOT_SEARCH_DIR ${_ENV_GAFFER_ROOT})
  endif()
endif()

# ------------------------------------------------------------------------
#  Search for Gaffer include DIR
# ------------------------------------------------------------------------

set(_GAFFER_INCLUDE_SEARCH_DIRS "")
list(APPEND _GAFFER_INCLUDE_SEARCH_DIRS
  ${GAFFER_INCLUDEDIR}
  ${_GAFFER_ROOT_SEARCH_DIR})

# Look for a standard Gaffer header file.
find_path(Gaffer_INCLUDE_DIR 
  Gaffer/Version.h
  NO_DEFAULT_PATH
  PATHS ${_GAFFER_INCLUDE_SEARCH_DIRS}
  PATH_SUFFIXES include
  DOC "Gaffer include path")

if(Gaffer_INCLUDE_DIR)
  # Get the Gaffer version from ai_version.h
  file(STRINGS ${Gaffer_INCLUDE_DIR}/Gaffer/Version.h version-file
         REGEX "#define[ \t]GAFFER_(MILESTONE|MAJOR|MINOR|PATCH).*")
  if(NOT version-file) 
    message(AUTHOR_WARNING "Gaffer_INCLUDE_DIR found, but Version.h is missing")
  endif()
  list(GET version-file 0 milestone-line)
  list(GET version-file 1 major-line)
  list(GET version-file 2 minor-line)
  list(GET version-file 3 patch-line)
  string(REGEX REPLACE "^#define[ \t]+GAFFER_MILESTONE_VERSION[ \t]+([0-9]+)$" "\\1" Gaffer_VERSION_MILESTONE ${milestone-line})
  string(REGEX REPLACE "^#define[ \t]+GAFFER_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" Gaffer_VERSION_MAJOR ${major-line})
  string(REGEX REPLACE "^#define[ \t]+GAFFER_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" Gaffer_VERSION_MINOR ${minor-line})
  string(REGEX REPLACE "^#define[ \t]+GAFFER_PATCH_VERSION[ \t]+([0-9]+)$" "\\1" Gaffer_VERSION_PATCH ${patch-line})
  set(Gaffer_VERSION ${Gaffer_VERSION_MILESTONE}.${Gaffer_VERSION_MAJOR}.${Gaffer_VERSION_MINOR}.${Gaffer_VERSION_PATCH} CACHE STRING "Gaffer Version")
endif()

# ------------------------------------------------------------------------
#  Search for Gaffer lib DIR
# ------------------------------------------------------------------------

# Append to _GAFFER_LIBRARYDIR_SEARCH_DIRS in priority order.
set(_GAFFER_LIBRARYDIR_SEARCH_DIRS "")
list(APPEND _GAFFER_LIBRARYDIR_SEARCH_DIRS
  ${GAFFER_LIBRARYDIR}
  ${_GAFFER_ROOT_SEARCH_DIR})

set(_GAFFER_INCLUDE_DIRS ${Gaffer_INCLUDE_DIR})

set(Gaffer_LIB_COMPONENTS "")
foreach(COMPONENT ${Gaffer_FIND_COMPONENTS})
  set(_COMPONENT ${COMPONENT})
  if(${COMPONENT} STREQUAL "boost_python")
    if(NOT PYTHON_VERSION) 
      message(FATAL "PYTHON_VERSION must be provided")
    endif()
    string(REPLACE "." "" _PYTHON_VERSION_STRIPPED ${PYTHON_VERSION})
    set(_COMPONENT "boost_python${_PYTHON_VERSION_STRIPPED}")
  elseif(${COMPONENT} STREQUAL "python")
    if(NOT PYTHON_VERSION) 
      message(FATAL "PYTHON_VERSION must be provided")
    endif()
    string(REPLACE "3.7" "3.7m" _PYTHON_FOLDER ${PYTHON_VERSION})
    set(_COMPONENT "python${_PYTHON_FOLDER}")
    list(APPEND _GAFFER_INCLUDE_DIRS ${Gaffer_INCLUDE_DIR}/python${_PYTHON_FOLDER})
  endif()

  find_library(Gaffer_${COMPONENT}_LIBRARY 
    ${_COMPONENT}
    NO_DEFAULT_PATH
    PATHS ${_GAFFER_LIBRARYDIR_SEARCH_DIRS}
    PATH_SUFFIXES lib
    DOC "Gaffer library")
  list(APPEND Gaffer_LIB_COMPONENTS ${Gaffer_${COMPONENT}_LIBRARY})

  if(Gaffer_${COMPONENT}_LIBRARY)
    set(Gaffer_${COMPONENT}_FOUND TRUE)
  else()
    set(Gaffer_${COMPONENT}_FOUND FALSE)
  endif()
endforeach()

# ------------------------------------------------------------------------
#  Cache and set Gaffer_FOUND
# ------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Gaffer
  FOUND_VAR Gaffer_FOUND
  REQUIRED_VARS
  Gaffer_INCLUDE_DIR
  Gaffer_LIB_COMPONENTS
  VERSION_VAR Gaffer_VERSION
  HANDLE_COMPONENTS)

if(Gaffer_FOUND)
  set(Gaffer_LIBRARIES ${Gaffer_LIB_COMPONENTS})

  set(Gaffer_LIBRARY_DIRS "")
  foreach(LIB ${Gaffer_LIB_COMPONENTS})
    get_filename_component(_GAFFER_LIBDIR ${LIB} DIRECTORY)
    list(APPEND Gaffer_LIBRARY_DIRS ${_GAFFER_LIBDIR})
  endforeach()
  list(REMOVE_DUPLICATES Gaffer_LIBRARY_DIRS)

  # Configure imported targets.
  foreach(COMPONENT ${Gaffer_FIND_COMPONENTS})
    if(NOT TARGET Gaffer::${COMPONENT})
      add_library(Gaffer::${COMPONENT} SHARED IMPORTED)
      set_target_properties(Gaffer::${COMPONENT} PROPERTIES
        IMPORTED_LOCATION "${Gaffer_${COMPONENT}_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Gaffer_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${_GAFFER_INCLUDE_DIRS}"
        IMPORTED_NO_SONAME TRUE)
    endif()
  endforeach()
elseif(Gaffer_FIND_REQUIRED)
  message(FATAL_ERROR "Unable to find Gaffer")
endif()