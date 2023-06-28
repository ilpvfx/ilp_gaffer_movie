#pragma once

#include <cstddef>// std::size_t

#include <ilp_movie/ilp_movie_export.hpp>

namespace ilp_movie {

// These are pixel formats used when reading/writing frames, i.e. when providing data to a muxer or
// reading frames from a demuxer. Note that codecs use other pixel formats internally, typically in
// YUV space. Always float32 values in [0..1].
enum class PixelFormat : int {
  kGray,
  kRGB,
  kRGBA,
  kNone,
};

enum class Channel : int {
  kGray,// NOTE(tohi): Could just use red?
  kRed,
  kGreen,
  kBlue,
  kAlpha,
};

// A non-zero value is returned on success. Zero is returned if the provided pixel format is not
// recognized by this function.
[[nodiscard]] ILP_MOVIE_EXPORT constexpr auto ChannelCount(const PixelFormat pix_fmt) noexcept
  -> std::size_t
{
  std::size_t c = 0U;
  // clang-format off
  switch (pix_fmt) {
  case PixelFormat::kGray: c = 1; break;
  case PixelFormat::kRGB:  c = 3; break;
  case PixelFormat::kRGBA: c = 4; break;
  default: break;
  }
  // clang-format on
  return c;
}

}// namespace ilp_movie
