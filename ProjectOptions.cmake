include(cmake/SystemLink.cmake)
#include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


macro(ilp_gaffer_movie_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*"))
    # NOTE(tohi): Disabling because cannot find libasan/libubsan
    #
    set(SUPPORTS_UBSAN OFF)
    # set(SUPPORTS_UBSAN ON)
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()
endmacro()

macro(ilp_gaffer_movie_setup_options)
  option(ilp_gaffer_movie_ENABLE_COVERAGE "Enable coverage reporting" OFF)

  # NOTE(tohi): Skip hardening for now...
  #
  # option(ilp_gaffer_movie_ENABLE_HARDENING "Enable hardening" ON)
  # cmake_dependent_option(
  #   ilp_gaffer_movie_ENABLE_GLOBAL_HARDENING
  #   "Attempt to push hardening options to built dependencies"
  #   ON
  #   ilp_gaffer_movie_ENABLE_HARDENING
  #   OFF)

  # NOTE(tohi): Skip sanitizers for now...
  #
  # ilp_gaffer_movie_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR ilp_gaffer_movie_PACKAGING_MAINTAINER_MODE)
    option(ilp_gaffer_movie_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(ilp_gaffer_movie_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(ilp_gaffer_movie_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(ilp_gaffer_movie_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(ilp_gaffer_movie_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(ilp_gaffer_movie_ENABLE_PCH "Enable precompiled headers" OFF)
    option(ilp_gaffer_movie_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(ilp_gaffer_movie_ENABLE_IPO "Enable IPO/LTO" ON)
    option(ilp_gaffer_movie_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(ilp_gaffer_movie_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(ilp_gaffer_movie_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(ilp_gaffer_movie_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(ilp_gaffer_movie_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(ilp_gaffer_movie_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(ilp_gaffer_movie_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(ilp_gaffer_movie_ENABLE_PCH "Enable precompiled headers" OFF)
    option(ilp_gaffer_movie_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      ilp_gaffer_movie_ENABLE_IPO
      ilp_gaffer_movie_WARNINGS_AS_ERRORS
      ilp_gaffer_movie_ENABLE_USER_LINKER
      ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS
      ilp_gaffer_movie_ENABLE_SANITIZER_LEAK
      ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED
      ilp_gaffer_movie_ENABLE_SANITIZER_THREAD
      ilp_gaffer_movie_ENABLE_SANITIZER_MEMORY
      ilp_gaffer_movie_ENABLE_UNITY_BUILD
      ilp_gaffer_movie_ENABLE_CLANG_TIDY
      ilp_gaffer_movie_ENABLE_CPPCHECK
      ilp_gaffer_movie_ENABLE_COVERAGE
      ilp_gaffer_movie_ENABLE_PCH
      ilp_gaffer_movie_ENABLE_CACHE)
  endif()

  # ilp_gaffer_movie_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  # if(LIBFUZZER_SUPPORTED AND (ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS OR ilp_gaffer_movie_ENABLE_SANITIZER_THREAD OR ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED))
  #   set(DEFAULT_FUZZER ON)
  # else()
  #   set(DEFAULT_FUZZER OFF)
  # endif()
  # option(ilp_gaffer_movie_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(ilp_gaffer_movie_global_options)
  if(ilp_gaffer_movie_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    ilp_gaffer_movie_enable_ipo()
  endif()

  # NOTE(tohi): Skip sanitizers for now...
  #
  # ilp_gaffer_movie_supports_sanitizers()

  # NOTE(tohi): Skip hardening for now...
  #
  # if(ilp_gaffer_movie_ENABLE_HARDENING AND ilp_gaffer_movie_ENABLE_GLOBAL_HARDENING)
  #   include(cmake/Hardening.cmake)
  #   if(NOT SUPPORTS_UBSAN 
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_THREAD
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_LEAK)
  #     set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
  #   else()
  #     set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
  #   endif()
  #   message("${ilp_gaffer_movie_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED}")
  #   ilp_gaffer_movie_enable_hardening(ilp_gaffer_movie_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  # endif()
endmacro()

macro(ilp_gaffer_movie_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(ilp_gaffer_movie_warnings INTERFACE)
  add_library(ilp_gaffer_movie_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  ilp_gaffer_movie_set_project_warnings(
    ilp_gaffer_movie_warnings
    ${ilp_gaffer_movie_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  # NOTE(tohi): Use default linker for now...
  #
  # if(ilp_gaffer_movie_ENABLE_USER_LINKER)
  #   include(cmake/Linker.cmake)
  #   configure_linker(ilp_gaffer_movie_options)
  # endif()

  # NOTE(tohi): Skip sanitizers for now...
  #
  # include(cmake/Sanitizers.cmake)
  # ilp_gaffer_movie_enable_sanitizers(
  #   ilp_gaffer_movie_options
  #   ${ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS}
  #   ${ilp_gaffer_movie_ENABLE_SANITIZER_LEAK}
  #   ${ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED}
  #   ${ilp_gaffer_movie_ENABLE_SANITIZER_THREAD}
  #   ${ilp_gaffer_movie_ENABLE_SANITIZER_MEMORY})

  # NOTE(tohi): Skip unity build for now...
  #
  # set_target_properties(ilp_gaffer_movie_options PROPERTIES UNITY_BUILD ${ilp_gaffer_movie_ENABLE_UNITY_BUILD})

  if(ilp_gaffer_movie_ENABLE_PCH)
    target_precompile_headers(
      ilp_gaffer_movie_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(ilp_gaffer_movie_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    ilp_gaffer_movie_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(ilp_gaffer_movie_ENABLE_CLANG_TIDY)
    ilp_gaffer_movie_enable_clang_tidy(ilp_gaffer_movie_options ${ilp_gaffer_movie_WARNINGS_AS_ERRORS})
  endif()

  if(ilp_gaffer_movie_ENABLE_CPPCHECK)
    ilp_gaffer_movie_enable_cppcheck(${ilp_gaffer_movie_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(ilp_gaffer_movie_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    ilp_gaffer_movie_enable_coverage(ilp_gaffer_movie_options)
  endif()

  if(ilp_gaffer_movie_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(ilp_gaffer_movie_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  # NOTE(tohi): Skip hardening for now...
  #
  # if(ilp_gaffer_movie_ENABLE_HARDENING AND NOT ilp_gaffer_movie_ENABLE_GLOBAL_HARDENING)
  #   include(cmake/Hardening.cmake)
  #   if(NOT SUPPORTS_UBSAN 
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_UNDEFINED
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_ADDRESS
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_THREAD
  #      OR ilp_gaffer_movie_ENABLE_SANITIZER_LEAK)
  #     set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
  #   else()
  #     set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
  #   endif()
  #   ilp_gaffer_movie_enable_hardening(ilp_gaffer_movie_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  # endif()

endmacro()
