# ---- Dependencies ----

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

# NOTE(tohi): Only for executable...
# ----- 
#
# Provide a simple smoke test to make sure that the CLI works and can display a --help message
# add_test(NAME cli.has_help COMMAND intro --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
# add_test(NAME cli.version_matches COMMAND intro --version)
# set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")
#
# ----- 

add_executable(mux_test mux_test.cpp)
target_link_libraries(mux_test 
  PRIVATE ilp_gaffer_movie::ilp_gaffer_movie_warnings
          ilp_gaffer_movie::ilp_gaffer_movie_options
          ilp_movie::ilp_movie
          Catch2::Catch2WithMain)

# Automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries.
catch_discover_tests(
  mux_test 
  TEST_PREFIX
  "mux_test."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "mux_test."
  OUTPUT_SUFFIX
  .xml)

add_executable(demux_test demux_test.cpp)
target_link_libraries(demux_test 
  PRIVATE ilp_gaffer_movie::ilp_gaffer_movie_warnings
          ilp_gaffer_movie::ilp_gaffer_movie_options
          ilp_movie::ilp_movie
          Catch2::Catch2WithMain)

# Automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries.
catch_discover_tests(
  demux_test 
  TEST_PREFIX
  "demux_test."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "demux_test."
  OUTPUT_SUFFIX
  .xml)

add_executable(decoder_test decoder_test.cpp)
target_link_libraries(decoder_test 
  PRIVATE ilp_gaffer_movie::ilp_gaffer_movie_warnings
          ilp_gaffer_movie::ilp_gaffer_movie_options
          ilp_movie::ilp_movie
          Catch2::Catch2WithMain)

# Automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries.
catch_discover_tests(
  decoder_test 
  TEST_PREFIX
  "decoder_test."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "decoder_test."
  OUTPUT_SUFFIX
  .xml)
  