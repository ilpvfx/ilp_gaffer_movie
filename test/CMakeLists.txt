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

add_executable(tests tests.cpp)
target_link_libraries(tests
  PRIVATE ilp_gaffer_movie::ilp_gaffer_movie_warnings
          ilp_gaffer_movie::ilp_gaffer_movie_options
          IlpGafferMovie
          Catch2::Catch2WithMain)

# Automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries.
catch_discover_tests(
  tests
  TEST_PREFIX
  "unittests."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)

# Add a file containing a set of constexpr tests.
add_executable(constexpr_tests constexpr_tests.cpp)
target_link_libraries(constexpr_tests
  PRIVATE ilp_gaffer_movie::ilp_gaffer_movie_warnings
          ilp_gaffer_movie::ilp_gaffer_movie_options
          IlpGafferMovie
          Catch2::Catch2WithMain)

catch_discover_tests(
  constexpr_tests
  TEST_PREFIX
  "constexpr."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "constexpr."
  OUTPUT_SUFFIX
  .xml)

# Disable the constexpr portion of the test, and build again this allows us to have an executable that we can debug when
# things go wrong with the constexpr testing
add_executable(relaxed_constexpr_tests constexpr_tests.cpp)
target_link_libraries(relaxed_constexpr_tests
  PRIVATE ilp_gaffer_movie::ilp_gaffer_movie_warnings
          ilp_gaffer_movie::ilp_gaffer_movie_options
          IlpGafferMovie
          Catch2::Catch2WithMain)
target_compile_definitions(relaxed_constexpr_tests PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE)

catch_discover_tests(
  relaxed_constexpr_tests
  TEST_PREFIX
  "relaxed_constexpr."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "relaxed_constexpr."
  OUTPUT_SUFFIX
  .xml)
