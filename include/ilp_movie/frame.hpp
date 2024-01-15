#pragma once

#include <array>// std::array
#include <cstddef>// std::byte
#include <cstdint>// uint8_t, int64_t, etc
#include <memory>// std::unique_ptr
#include <optional>// std::optional
#include <type_traits>// std::is_same_v, std::decay_t

#include "ilp_movie/ilp_movie_export.hpp"

namespace ilp_movie {

namespace PixFmt {
  constexpr const char *kRGB_P_F32 = "gbrpf32le";
  constexpr const char *kRGBA_P_F32 = "gbrapf32le";
}// namespace PixFmt

namespace ColorRange {
  // Limited range.
  constexpr const char *kTv = "tv";

  // Full range.
  constexpr const char *kPc = "pc";

  constexpr const char *kUnknown = "unknown";
}// namespace ColorRange

namespace Colorspace {
  constexpr const char *kBt709 = "bt709";
}// namespace Colorspace

namespace ColorTrc {
  constexpr const char *kIec61966_2_1 = "iec61966-2-1";
}// namespace ColorTrc

namespace ColorPrimaries {
  constexpr const char *kBt709 = "bt709";
}// namespace ColorPrimaries

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

  // Pointers to the picture/channel planes.
  std::array<uint8_t *, 4> data = {};

  // [bytes]
  std::array<int, 4> linesize = {};

  // Views don't own their buffers.
  const uint8_t *buf = nullptr;
};

// Returns the size, in [bytes], of the buffer needed for the given pixel format and
// pixel dimensions.
[[nodiscard]] ILP_MOVIE_EXPORT auto GetBufferSize(const char *pix_fmt_name,
  int width,
  int height) noexcept -> std::optional<std::size_t>;

// Can, and should, be used for both frames and views.
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

// Try to access (typed) pixel data for a component. Returns an empty PixelData if this is not
// possible.
//
// Implemented for:
// - <float>
// - <const float>
//
// NOTE: Other types will give linker errors.
template<typename PixelT>
[[nodiscard]] ILP_MOVIE_EXPORT auto CompPixelData(const std::array<uint8_t *, 4> &data,
  const std::array<int, 4> &linesize,
  Comp::ValueType c,
  int height,
  const char *pix_fmt_name) noexcept -> PixelData<PixelT>;

// Convenience function for accessing component pixel data from a frame (view).
template<typename PixelT, typename FrameT>
[[nodiscard]] ILP_MOVIE_EXPORT auto CompPixelData(const FrameT &frame,
  const Comp::ValueType c) noexcept -> PixelData<PixelT>
{
  static_assert(
    std::is_same_v<std::decay_t<FrameT>, Frame> || std::is_same_v<std::decay_t<FrameT>, FrameView>);

  return CompPixelData<PixelT>(
    frame.data, frame.linesize, c, frame.hdr.height, frame.hdr.pix_fmt_name);
}

}// namespace ilp_movie
