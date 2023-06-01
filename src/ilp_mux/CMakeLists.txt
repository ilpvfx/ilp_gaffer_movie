include(GenerateExportHeader)

add_library(ilp_mux SHARED
  mux.cpp)
add_library(ilp_gaffer_movie::ilp_mux 
  ALIAS ilp_mux)

target_link_libraries(ilp_mux 
  PUBLIC 
    PkgConfig::LIBAV)
target_link_libraries(ilp_mux 
  PRIVATE 
    ilp_gaffer_movie_options 
    ilp_gaffer_movie_warnings)

target_include_directories(ilp_mux ${WARNING_GUARD} 
  PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(ilp_mux 
  PUBLIC 
    cxx_std_17)

target_compile_definitions(ilp_mux
  PUBLIC 
    _GLIBCXX_USE_CXX11_ABI=0
)

set_target_properties(ilp_mux
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    #CXX_VISIBILITY_PRESET hidden
    #VISIBILITY_INLINES_HIDDEN YES
    POSITION_INDEPENDENT_CODE ON # Always PIC
)

generate_export_header(ilp_mux
  EXPORT_FILE_NAME 
    ${PROJECT_BINARY_DIR}/include/ilp_mux/ilp_mux_export.hpp)

target_include_directories(ilp_mux PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(ilp_mux PUBLIC ILP_MUX_STATIC_DEFINE)
endif()

# 
# Install.
#
install(TARGETS ilp_mux DESTINATION lib)