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
    find_package(PkgConfig REQUIRED)
    message(STATUS "FFMPEG_ROOT: ${FFMPEG_ROOT}")
    set(ENV{PKG_CONFIG_PATH} "${FFMPEG_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavcodec
        libavdevice
        libavfilter
        libavformat
        libavutil
        libpostproc
        libswresample
        libswscale)
  endif()

  #
  # Find Catch2.
  #
  if(NOT TARGET Catch2::Catch2WithMain)
    CPMAddPackage("gh:catchorg/Catch2@3.3.2")
  endif()

endfunction()
