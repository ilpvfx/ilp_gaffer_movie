#pragma once

#include <string>
#include <utility>// std::pair
#include <vector>

#include <ilp_movie/ilp_movie_export.hpp>

struct AVDictionary;

namespace dict_utils_internal {

// Simple RAII class for AVDictionary.
class Options
{
public:
  Options(const Options &) noexcept = default;
  Options &operator=(const Options &) noexcept = default;
  Options(Options &&) noexcept = default;
  Options &operator=(Options &&) noexcept = default;
  
  ~Options() noexcept;

  AVDictionary *dict = nullptr;
};

[[nodiscard]] ILP_MOVIE_NO_EXPORT auto Count(const Options &opt) noexcept -> int;

[[nodiscard]] ILP_MOVIE_NO_EXPORT auto Empty(const Options &opt) noexcept -> bool;

[[nodiscard]] ILP_MOVIE_NO_EXPORT auto Entries(const Options &opt) noexcept
  -> std::vector<std::pair<std::string, std::string>>;

}// namespace dict_utils_internal