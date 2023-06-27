include(GenerateExportHeader)

add_library(IlpGafferMovieModule SHARED 
  ilp_gaffer_movie_module.cpp)
add_library(ilp_gaffer_movie::IlpGafferMovieModule ALIAS IlpGafferMovieModule)  
target_link_libraries(IlpGafferMovieModule 
  PRIVATE
    ilp_gaffer_movie_options
    ilp_gaffer_movie_warnings)
target_include_directories(IlpGafferMovieModule ${WARNING_GUARD}
  PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)
target_include_directories(IlpGafferMovieModule 
  PRIVATE 
    "${CMAKE_BINARY_DIR}/configured_files/include")
target_compile_features(IlpGafferMovieModule 
  PUBLIC cxx_std_17)
set_target_properties(IlpGafferMovieModule
  PROPERTIES VERSION ${PROJECT_VERSION}
             PREFIX "" 
             OUTPUT_NAME "_IlpGafferMovie"
             #  CXX_VISIBILITY_PRESET hidden
             #  VISIBILITY_INLINES_HIDDEN YES
             POSITION_INDEPENDENT_CODE ON # Always PIC
)
target_compile_definitions(IlpGafferMovieModule 
  PRIVATE 
    BOOST_SIGNALS_NO_DEPRECATION_WARNING=1 
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    _GLIBCXX_USE_CXX11_ABI=0)
# if(NOT BUILD_SHARED_LIBS)
#   target_compile_definitions(IlpGafferMovieModule 
#     PUBLIC 
#       ILPGAFFERMOVIEMODULE_STATIC_DEFINE)
# endif()
target_link_libraries(IlpGafferMovieModule
  PUBLIC
    IlpGafferMovie)
target_link_system_libraries(IlpGafferMovieModule 
  PUBLIC
    Gaffer::GafferBindings 
    Gaffer::IECorePython 
    Gaffer::boost_python
    Gaffer::python)
generate_export_header(IlpGafferMovieModule 
  EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/ilp_gaffer_movie/ilp_gaffer_movie_module_export.hpp)
install(TARGETS IlpGafferMovieModule DESTINATION python/IlpGafferMovie)
