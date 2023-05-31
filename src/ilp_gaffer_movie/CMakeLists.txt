include(GenerateExportHeader)

add_library(ilp_gaffer_movie SHARED
  movie_reader.cpp
  movie_writer.cpp)
add_library(ilp_gaffer_movie::ilp_gaffer_movie 
  ALIAS ilp_gaffer_movie)

target_link_libraries(ilp_gaffer_movie 
  PUBLIC 
    Gaffer::Gaffer
    GafferScene 
    IECore)
target_link_libraries(ilp_gaffer_movie 
  PRIVATE 
    ilp_gaffer_movie_options 
    ilp_gaffer_movie_warnings)

target_include_directories(ilp_gaffer_movie ${WARNING_GUARD} 
  PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(ilp_gaffer_movie 
  PUBLIC 
    cxx_std_17)

target_compile_definitions(ilp_gaffer_movie
  PUBLIC 
    BOOST_SIGNALS_NO_DEPRECATION_WARNING=1 
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    _GLIBCXX_USE_CXX11_ABI=0
)

set_target_properties(
  ilp_gaffer_movie
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    #CXX_VISIBILITY_PRESET hidden
    #VISIBILITY_INLINES_HIDDEN YES
    POSITION_INDEPENDENT_CODE ON # Always PIC
)

generate_export_header(ilp_gaffer_movie 
  EXPORT_FILE_NAME 
    ${PROJECT_BINARY_DIR}/include/ilp_gaffer_movie/ilp_gaffer_movie_export.hpp)

target_include_directories(ilp_gaffer_movie PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

# if(NOT BUILD_SHARED_LIBS)
#   target_compile_definitions(ilp_gaffer_movie PUBLIC ILP_GAFFER_MOVIE_STATIC_DEFINE)
# endif()

# 
# Install.
#
install(TARGETS ilp_gaffer_movie DESTINATION lib)
