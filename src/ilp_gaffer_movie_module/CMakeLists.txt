#
# Build the python bindings.
#
string(REPLACE "." "" PYTHON_VERSION_STRIPPED ${PYTHON_VERSION})
string(REPLACE "3.7" "3.7m" PYTHON_FOLDER ${PYTHON_VERSION})

message(STATUS "PYTHON_VERSION: ${PYTHON_VERSION}")
message(STATUS "PYTHON_FOLDER: ${PYTHON_FOLDER}")

add_library(ilp_gaffer_movie_reader SHARED
  ilp_gaffer_movie_reader_module.cpp)
add_library(ilp_gaffer_movie::ilp_gaffer_movie_reader 
  ALIAS ilp_gaffer_movie_reader)

target_link_libraries(ilp_gaffer_movie_reader 
  PRIVATE
    ilp_gaffer_movie::ilp_gaffer_movie
    Gaffer::Gaffer
    GafferBindings 
    IECorePython 
    boost_python${PYTHON_VERSION_STRIPPED} 
    python${PYTHON_FOLDER}
)
# target_link_directories(ilp_gaffer_movie_reader 
#   PUBLIC 
#     ${GAFFER_ROOT}/lib
# )
target_include_directories(ilp_gaffer_movie_reader 
  SYSTEM PUBLIC
    ${GAFFER_ROOT}/include 
    ${GAFFER_ROOT}/include/python${PYTHON_FOLDER}
)

target_include_directories(ilp_gaffer_movie_reader ${WARNING_GUARD} 
  PUBLIC 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

# target_include_directories(ilp_gaffer_movie_reader
#   PUBLIC 
#     include 
# )
target_compile_definitions(ilp_gaffer_movie_reader 
  PRIVATE 
    BOOST_SIGNALS_NO_DEPRECATION_WARNING=1 
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    _GLIBCXX_USE_CXX11_ABI=0
)
set_target_properties(ilp_gaffer_movie_reader 
  PROPERTIES 
    VERSION ${PROJECT_VERSION}
    # CXX_STANDARD ${CMAKE_CXX_STANDARD}
    # CXX_STANDARD_REQUIRED ON
    #CXX_VISIBILITY_PRESET default
    #VISIBILITY_INLINES_HIDDEN NO
    POSITION_INDEPENDENT_CODE ON # Always PIC
    PREFIX "" 
    OUTPUT_NAME "_IlpGafferMovie"
)


# 
# Install.
#
install(TARGETS ilp_gaffer_movie_reader DESTINATION python/IlpGafferMovie)
install(DIRECTORY python DESTINATION . FILES_MATCHING PATTERN "*.py")
install(DIRECTORY startup DESTINATION . FILES_MATCHING PATTERN "*.py")
