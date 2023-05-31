# FindGaffer.cmake
#
# Finds Gaffer
#
# This will define the following variables
#
#    Gaffer_FOUND
#    Gaffer_INCLUDE_DIR
#    Gaffer_LIBRARIES
#
# and the following imported targets
#
#     Gaffer::Gaffer
#
# Based on https://izzys.casa/2020/12/how-to-find-packages-with-cmake-the-basics/
#
# Author: Tommy Hinks, tommy.hinks@ilpvfx.com

include(FindPackageHandleStandardArgs)

find_library(Gaffer_LIBRARIES
        NAMES Gaffer 
        PATHS ${GAFFER_ROOT}/lib $ENV{GAFFER_ROOT}/lib
        DOC "Gaffer library")
find_path(Gaffer_INCLUDE_DIR 
        NAMES Gaffer/Version.h
        PATHS ${GAFFER_ROOT}/include $ENV{GAFFER_ROOT}/include
        DOC "Gaffer include path")

message(STATUS "${Gaffer_LIBRARIES}")

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

find_package_handle_standard_args(Gaffer
  REQUIRED_VARS Gaffer_INCLUDE_DIR Gaffer_LIBRARIES
  VERSION_VAR Gaffer_VERSION
  HANDLE_COMPONENTS)

if(Gaffer_FOUND)
  mark_as_advanced(Gaffer_INCLUDE_DIR)
  mark_as_advanced(Gaffer_LIBRARIES)
  mark_as_advanced(Gaffer_VERSION)
endif()

if(Gaffer_FOUND AND NOT TARGET Gaffer::Gaffer)
    add_library(Gaffer::Gaffer SHARED IMPORTED GLOBAL)
    set_property(TARGET Gaffer::Gaffer PROPERTY IMPORTED_LOCATION ${Gaffer_LIBRARIES})
    set_property(TARGET Gaffer::Gaffer PROPERTY IMPORTED_NO_SONAME TRUE)
    set_property(TARGET Gaffer::Gaffer PROPERTY VERSION ${Gaffer_VERSION})
    target_include_directories(Gaffer::Gaffer SYSTEM INTERFACE ${Gaffer_INCLUDE_DIR})
    #add_library(Gaffer::Gaffer SHARED IMPORTED GLOBAL)
    #set_target_properties(Gaffer::Gaffer PROPERTIES IMPORTED_NO_SONAME TRUE)
    #set_target_properties(Gaffer::Gaffer PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${ARNOLD_INCLUDE_DIR})

    # TODO: Setup targets Gaffer::GafferScene, etc.
endif()
