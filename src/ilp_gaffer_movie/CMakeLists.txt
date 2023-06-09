include(GenerateExportHeader)

add_library(IlpGafferMovie SHARED 
  movie_reader.cpp
  movie_writer.cpp)
add_library(ilp_gaffer_movie::IlpGafferMovie ALIAS IlpGafferMovie)  
target_link_libraries(IlpGafferMovie 
  PRIVATE
    ilp_gaffer_movie_options
    ilp_gaffer_movie_warnings)
target_include_directories(IlpGafferMovie ${WARNING_GUARD}
  PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)
target_include_directories(IlpGafferMovie 
  PRIVATE 
    "${CMAKE_BINARY_DIR}/configured_files/include")
target_compile_features(IlpGafferMovie 
  PUBLIC cxx_std_17)
set_target_properties(IlpGafferMovie
  PROPERTIES VERSION ${PROJECT_VERSION}
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES
             POSITION_INDEPENDENT_CODE ON # Always PIC
)
target_compile_definitions(IlpGafferMovie 
  PRIVATE 
    BOOST_SIGNALS_NO_DEPRECATION_WARNING=1 
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    _GLIBCXX_USE_CXX11_ABI=0)
# if(NOT BUILD_SHARED_LIBS)
#   target_compile_definitions(IlpGafferMovie 
#     PUBLIC 
#       ILPGAFFERMOVIE_STATIC_DEFINE)
# endif()
target_link_libraries(IlpGafferMovie
  PUBLIC
    ilp_mux)
target_link_system_libraries(IlpGafferMovie 
  PUBLIC
    Gaffer::Gaffer 
    Gaffer::GafferScene 
    Gaffer::IECore)

generate_export_header(IlpGafferMovie 
  EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/ilp_gaffer_movie/ilp_gaffer_movie_export.hpp)
  
install(TARGETS IlpGafferMovie DESTINATION lib)
