cmake_minimum_required(VERSION 3.21)

# Only set the cxx_standard if it is not set by someone else.
if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# Strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++17 and -std=gnu++17 for example
# when compiling with PCH enabled.
set(CMAKE_CXX_EXTENSIONS OFF)

file(STRINGS VERSION.txt version)

# Set the project name and language.
project(
  IlpGafferMovie
  VERSION ${version}
  DESCRIPTION ""
  #HOMEPAGE_URL "%%myurl%%"
  LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)

ilp_gaffer_movie_setup_options()
ilp_gaffer_movie_global_options()

include(Dependencies.cmake)
ilp_gaffer_movie_setup_dependencies()

ilp_gaffer_movie_local_options()

# Don't know if this should be set globally from here or not...
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(GIT_SHA
    "Unknown"
    CACHE STRING "SHA this build was generated from")
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)
message(STATUS "GIT_SHA: ${GIT_SHA}")

target_compile_features(ilp_gaffer_movie_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(ilp_gaffer_movie::ilp_gaffer_movie_options ALIAS ilp_gaffer_movie_options)
add_library(ilp_gaffer_movie::ilp_gaffer_movie_warnings ALIAS ilp_gaffer_movie_warnings)

# Configure files based on CMake configuration options:
add_subdirectory(configured_files)

# Adding the src:
add_subdirectory(src)

# Build the python modules.
install(DIRECTORY python DESTINATION . FILES_MATCHING PATTERN "*.py")

# Build the startup configs.
install(DIRECTORY startup DESTINATION . FILES_MATCHING PATTERN "*.py")  

# Don't even look at tests if we're not top level.
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

# Adding the tests:
include(CTest)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
