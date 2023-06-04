cmake_minimum_required(VERSION 3.21)

# Only set the cxx_standard if it is not set by someone else.
if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# strongly encouraged to enable this globally to avoid conflicts between
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
# include(ProjectOptions.cmake)

# ilp_gaffer_movie_setup_options()
# ilp_gaffer_movie_global_options()

include(Dependencies.cmake)
ilp_gaffer_movie_setup_dependencies()

# ilp_gaffer_movie_local_options()


# Build the library.
add_library(IlpGafferMovie SHARED 
  src/ilp_gaffer_movie/movie_reader.cpp
  src/ilp_gaffer_movie/movie_writer.cpp)
# set_target_properties(ilp_gaffer_movie 
#   PROPERTIES 
#     PREFIX "" 
#     OUTPUT_NAME "IlpGafferMovie")
target_compile_definitions(IlpGafferMovie 
  PRIVATE 
    BOOST_SIGNALS_NO_DEPRECATION_WARNING=1 
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(IlpGafferMovie 
  PRIVATE 
    include)
target_link_libraries(IlpGafferMovie 
  Gaffer::Gaffer 
  Gaffer::GafferScene 
  Gaffer::IECore)
install(TARGETS IlpGafferMovie DESTINATION lib)

# Build the python bindings.
add_library(IlpGafferMovieModule SHARED 
  src/ilp_gaffer_movie_module/ilp_gaffer_movie_module.cpp)
set_target_properties(IlpGafferMovieModule 
  PROPERTIES 
    PREFIX "" 
    OUTPUT_NAME "_IlpGafferMovie")
target_compile_definitions(IlpGafferMovieModule 
  PRIVATE 
    BOOST_SIGNALS_NO_DEPRECATION_WARNING=1 
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(IlpGafferMovieModule 
  PRIVATE 
    include)
target_link_libraries(IlpGafferMovieModule 
  IlpGafferMovie
  Gaffer::GafferBindings 
  Gaffer::IECorePython 
  Gaffer::boost_python
  Gaffer::python)
install(TARGETS IlpGafferMovieModule DESTINATION python/IlpGafferMovie)

# Build the python modules.
install(DIRECTORY python DESTINATION . FILES_MATCHING PATTERN "*.py")

# Build the startup configs.
install(DIRECTORY startup DESTINATION . FILES_MATCHING PATTERN "*.py")  
