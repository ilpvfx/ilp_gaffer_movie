#pragma once

#include <string_view>// std::string_view

namespace ilp_movie {

namespace ColorRange {
  // clang-format off
  constexpr auto kUnknown = std::string_view{ "unknown" };
  constexpr auto kTv      = std::string_view{ "tv" }; // Limited range.
  constexpr auto kPc      = std::string_view{ "pc" }; // Full range.
  // clang-format on
}// namespace ColorRange

namespace ColorPrimaries {
  constexpr auto kBt709 = std::string_view{ "bt709" };
}// namespace ColorPrimaries

namespace Colorspace {
  constexpr auto kBt709 = std::string_view{ "bt709" };
}// namespace Colorspace

// Transfer characteristics.
namespace ColorTrc {
  constexpr auto kIec61966_2_1 = std::string_view{ "iec61966-2-1" };
}// namespace ColorTrc

}// namespace ilp_movie
