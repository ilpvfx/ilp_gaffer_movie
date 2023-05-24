#include <catch2/catch_test_macros.hpp>

#include <ilp_gaffer_movie/movie_reader.hpp>

[[nodiscard]] static constexpr auto FactorialConstExpr(int input) noexcept -> int
{
  if (input == 0) { return 1; }

  return input * FactorialConstExpr(input - 1);
}

TEST_CASE("Factorials are computed with constexpr", "[factorial]")
{
  STATIC_REQUIRE(FactorialConstExpr(0) == 1);
  STATIC_REQUIRE(FactorialConstExpr(1) == 1);
  STATIC_REQUIRE(FactorialConstExpr(2) == 2);
  STATIC_REQUIRE(FactorialConstExpr(3) == 6);
  STATIC_REQUIRE(FactorialConstExpr(10) == 3628800);
}
