include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets.
function(ilp_gaffer_movie_setup_dependencies)
  # For each dependency, see if it has already been 
  # provided to us by a parent project.

  #
  # Find Gaffer.
  #
  if(NOT TARGET Gaffer::Gaffer)
    if(GAFFER_ROOT AND GAFFER_VERSION)
      message(FATAL_ERROR "Cannot provide GAFFER_ROOT and GAFFER_VERSION simultaneously")
    endif()
    if(NOT GAFFER_ROOT AND GAFFER_VERSION)
      CPMAddPackage(
        NAME gaffer
        URL https://github.com/GafferHQ/gaffer/releases/download/${GAFFER_VERSION}/gaffer-${GAFFER_VERSION}-linux.tar.gz
        VERSION ${GAFFER_VERSION}
        DOWNLOAD_ONLY TRUE)      
      set(GAFFER_ROOT ${gaffer_SOURCE_DIR})
    endif()
    message(STATUS "GAFFER_ROOT: ${GAFFER_ROOT}")
    find_package(Gaffer 
      REQUIRED 
      COMPONENTS
        Gaffer
        GafferBindings
        GafferScene
        IECore
        IECorePython
        boost_python 
        python)
  endif()

  #
  # Find FFmpeg.
  #
  if(NOT TARGET PkgConfig::LIBAV)
    # find_package(PkgConfig REQUIRED)
    # message(STATUS "FFMPEG_ROOT: ${FFMPEG_ROOT}")
    # set(ENV{PKG_CONFIG_PATH} "${FFMPEG_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    # pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    #     libavcodec
    #     libavdevice
    #     libavfilter
    #     libavformat
    #     libavutil
    #     libpostproc
    #     libswresample
    #     libswscale)
  endif()

  # if(NOT TARGET spdlog::spdlog)
  #   cpmaddpackage(
  #     NAME
  #     spdlog
  #     VERSION
  #     1.11.0
  #     GITHUB_REPOSITORY
  #     "gabime/spdlog"
  #     OPTIONS
  #     "SPDLOG_FMT_EXTERNAL ON")
  # endif()

  if(NOT TARGET Catch2::Catch2WithMain)
    CPMAddPackage("gh:catchorg/Catch2@3.3.2")
  endif()

  # if(NOT TARGET tools::tools)
  #   cpmaddpackage("gh:lefticus/tools#update_build_system")
  # endif()

endfunction()
