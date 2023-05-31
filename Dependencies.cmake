include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets.
function(ilp_gaffer_movie_setup_dependencies)

  # For each dependency, see if it's
  # already been provided to us by a parent project

  # if(NOT TARGET fmtlib::fmtlib)
  #   cpmaddpackage("gh:fmtlib/fmt#9.1.0")
  # endif()

  if(NOT TARGET Gaffer::Gaffer)
    CPMAddPackage(
      NAME gaffer
      URL https://github.com/GafferHQ/gaffer/releases/download/1.2.0.2/gaffer-1.2.0.2-linux.tar.gz
      VERSION 1.2.0.2
    )      
      #DOWNLOAD_ONLY True
#      NAME          
#        gaffer
#      URL 
#        "https://github.com/GafferHQ/gaffer/releases/download/1.2.0.2/gaffer-1.2.0.2-linux.tar.gz"
      # VERSION       
      #  1.2.0.2
      # GIT_TAG         
      #   1.2.0.2
      # GITHUB_REPOSITORY
      #   "GafferHQ/gaffer"
      # DOWNLOAD_ONLY 
      #   TRUE
#    )

    #set(GAFFER_ROOT ${gaffer_SOURCE_DIR})
    message(STATUS "GAFFER_ROOT: ${GAFFER_ROOT}")
    find_package(Gaffer)

    # Create imported target.
    # set(GAFFER_SHARED_LIBS
    #   "${gaffer_SOURCE_DIR}/lib/libGaffer" 
    #   "${gaffer_SOURCE_DIR}/lib/libGafferScene" 
    #   "${gaffer_SOURCE_DIR}/lib/libIECore" 
    # )    
    # message(STATUS "${GAFFER_SHARED_LIBS}")
    # add_library(gaffer::gaffer SHARED IMPORTED)
    # set_target_properties(gaffer::gaffer 
    #   PROPERTIES
    #     IMPORTED_LOCATION ${GAFFER_SHARED_LIBS}
    #     INTERFACE_INCLUDE_DIRECTORIES ${gaffer_SOURCE_DIR}/include
    #     INTERFACE_LINK_DIRECTORIES ${gaffer_SOURCE_DIR}/lib
    #     # INTERFACE_COMPILE_DEFINITIONS "ENABLE_FOO"
    # )
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
    cpmaddpackage("gh:catchorg/Catch2@3.3.2")
  endif()

  # if(NOT TARGET CLI11::CLI11)
  #   cpmaddpackage("gh:CLIUtils/CLI11@2.3.2")
  # endif()

  # if(NOT TARGET ftxui::screen)
  #   cpmaddpackage("gh:ArthurSonzogni/FTXUI#e23dbc7473654024852ede60e2121276c5aab660")
  # endif()

  # if(NOT TARGET tools::tools)
  #   cpmaddpackage("gh:lefticus/tools#update_build_system")
  # endif()

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

endfunction()
