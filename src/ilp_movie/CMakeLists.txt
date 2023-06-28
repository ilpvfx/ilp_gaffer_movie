include(GenerateExportHeader)

add_library(ilp_movie SHARED
  "demux.cpp"
  "log.cpp"
  "mux.cpp"
  "pixel_data.cpp"
  "internal/dict_utils.cpp"
  "internal/filter_graph.cpp"
  "internal/log_utils.cpp")
add_library(ilp_movie::ilp_movie ALIAS ilp_movie)
target_link_libraries(ilp_movie 
  PRIVATE 
    ilp_gaffer_movie_options 
    ilp_gaffer_movie_warnings)
target_include_directories(ilp_movie ${WARNING_GUARD} 
  PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)
target_include_directories(ilp_movie
  PRIVATE 
    "${CMAKE_BINARY_DIR}/configured_files/include")
target_include_directories(ilp_movie
  PRIVATE
    "${CMAKE_BINARY_DIR}/configured_files/include"
    ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(ilp_movie 
  PUBLIC cxx_std_17)
set_target_properties(ilp_movie
  PROPERTIES VERSION ${PROJECT_VERSION}
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  POSITION_INDEPENDENT_CODE ON # Always PIC
)
target_compile_definitions(ilp_movie
  PUBLIC 
    _GLIBCXX_USE_CXX11_ABI=0
)
# if(NOT BUILD_SHARED_LIBS)
#   target_compile_definitions(ilp_movie
#     PUBLIC 
#       ILP_VIDEO_STATIC_DEFINE)
# endif()
target_link_system_libraries(ilp_movie 
  PUBLIC 
    PkgConfig::LIBAV)

generate_export_header(ilp_movie
  EXPORT_FILE_NAME 
    ${PROJECT_BINARY_DIR}/include/ilp_movie/ilp_movie_export.hpp)
    
install(TARGETS ilp_movie DESTINATION lib)
