#include <iostream>// std::cout, std::cerr
#include <string>// std::string
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/demux.hpp>
#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>

// Helper function - has no effect on test results.
static void DumpLogLines(std::vector<std::string> *log_lines)
{
  for (auto &&line : *log_lines) { std::cerr << line; }
  log_lines->clear();
}

namespace {

TEST_CASE("test")
{
  std::vector<std::string> log_lines;
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.emplace_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  DumpLogLines(&log_lines);
}

}// namespace