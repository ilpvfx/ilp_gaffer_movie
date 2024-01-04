#pragma once

#include <array>// std::array
#include <cstddef>// std::byte
#include <cstdint>// uint8_t, int64_t, etc
#include <memory>// std::unique_ptr
#include <optional>// std::optional

#include "ilp_movie/ilp_movie_export.hpp"

namespace ilp_movie {

namespace PixFmt {
  constexpr const char *kRGB_P_F32 = "gbrpf32le";
  constexpr const char *kRGBA_P_F32 = "gbrapf32le";
}// namespace PixFmt

namespace Comp {
  using ValueType = int;
  constexpr ValueType kR = 0;
  constexpr ValueType kG = 1;
  constexpr ValueType kB = 2;
  constexpr ValueType kA = 3;

  constexpr ValueType kY = 0;
  constexpr ValueType kU = 1;
  constexpr ValueType kV = 2;

  constexpr ValueType kUnknown = -1;
}// namespace Comp

struct FrameHeader
{
  int width = -1;
  int height = -1;
  int64_t frame_nb = -1;
  bool key_frame = false;
  struct
  {
    int num = 0;
    int den = 1;
  } pixel_aspect_ratio = {};

  const char *pix_fmt_name = nullptr;
  const char *color_range_name = nullptr;
  const char *color_space_name = nullptr;
  const char *color_trc_name = nullptr;
  const char *color_primaries_name = nullptr;
};

struct Frame
{
  FrameHeader hdr = {};

  // Pointers to the picture/channel planes.
  std::array<uint8_t *, 4> data = {};

  // [bytes]
  std::array<int, 4> linesize = {};

  // Frames own their pixel data.
  std::unique_ptr<uint8_t[]> buf = nullptr;
};

struct FrameView
{
  FrameHeader hdr = {};

  std::array<uint8_t *, 4> data = {};

  std::array<int, 4> linesize = {};

  const uint8_t *buf = nullptr;
};

// In [bytes].
[[nodiscard]] ILP_MOVIE_EXPORT auto GetBufferSize(const char *pix_fmt_name,
  int width,
  int height) noexcept -> std::optional<std::size_t>;

[[nodiscard]] ILP_MOVIE_EXPORT auto CountPlanes(const char *pix_fmt_name) noexcept
  -> std::optional<int>;

[[nodiscard]] ILP_MOVIE_EXPORT auto FillArrays(std::array<uint8_t *, 4> &data,
  std::array<int, 4> &linesize,
  const uint8_t *buf,
  const char *pix_fmt_name,
  int width,
  int height) noexcept -> bool;

// Simple span.
template<typename PixelT> struct PixelData
{
  PixelT *data = nullptr;

  // Number of elements.
  std::size_t count = 0U;
};

template<typename PixelT>
[[nodiscard]] ILP_MOVIE_EXPORT constexpr auto Empty(const PixelData<PixelT> &p) noexcept -> bool
{
  return p.data == nullptr || p.count == 0;
}

// Try to access pixel data for a component. Returns an empty PixelData if this is not possible.
//
// Implemented for:
// - <float>
// - <const float>
//
// NOTE: Other types will give linker errors.
template<typename PixelT>
[[nodiscard]] ILP_MOVIE_EXPORT auto CompPixelData(const std::array<uint8_t *, 4> &data,
  Comp::ValueType c,
  int w,
  int h,
  const char *pix_fmt_name) noexcept -> PixelData<PixelT>;

}// namespace ilp_movie