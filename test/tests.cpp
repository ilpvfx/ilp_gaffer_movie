#include <catch2/catch_test_macros.hpp>

#include <ilp_gaffer_movie/movie_reader.hpp>

[[nodiscard]] static auto Factorial(int input) noexcept -> int
{
  int result = 1;

  while (input > 0) {
    result *= input;
    --input;
  }

  return result;
}

TEST_CASE("Factorials are computed", "[factorial]")
{
  REQUIRE(Factorial(0) == 1);
  REQUIRE(Factorial(1) == 1);
  REQUIRE(Factorial(2) == 2);
  REQUIRE(Factorial(3) == 6);
  REQUIRE(Factorial(10) == 3628800);
}
