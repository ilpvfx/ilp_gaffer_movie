#pragma once

#include <cstddef>// std::size_t
#include <functional>// std::function
#include <memory>// std::unique_ptr
#include <vector>// std::vector

#include <ilp_movie/ilp_movie_export.hpp>

namespace ilp_movie {

// Always float32 values [0..1].
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

struct DemuxImpl;
using demux_impl_ptr = std::unique_ptr<DemuxImpl, std::function<void(DemuxImpl *)>>;

struct DemuxParams
{
  const char *filename = nullptr;
  float frame_rate = -1.F;

  // The pixel format to set as the filter graph output, i.e. this will determine the format
  // of the frames we generate.
  PixelFormat pix_fmt = PixelFormat::kNone;
};

struct DemuxContext
{
  DemuxParams params = {};

  // Private implementation specific data stored as an opaque pointer.
  // The implementation specific data contains low-level resources such as
  // format context, codecs, etc. The resources are automatically freed when this
  // context is destroyed.
  demux_impl_ptr impl = nullptr;
};


// Simple span.
template<typename PixelT = const float> struct PixelData
{
  PixelT *data = nullptr;
  std::size_t count = 0U;// Number of elements.
};

struct DemuxFrame
{
  int width = -1;
  int height = -1;
  int frame_index = -1;
  double pixel_aspect_ratio = -1.0;

  PixelFormat pix_fmt = PixelFormat::kNone;

  // NOTE(tohi): If we wanted to support non-floating point formats as well this would need to be
  //             a byte buffer: std::vector<std::uint8_t> (or std::byte).
  //             Should probably be std::unique_ptr<std::uint8_t[]>, we are not doing vector
  //             operations on it?
  std::vector<float> buf;
};

[[nodiscard]] ILP_MOVIE_EXPORT auto MakeDemuxContext(const DemuxParams &params/*,
  DemuxFrame *first_frame*/) noexcept -> std::unique_ptr<DemuxContext>;

[[nodiscard]] ILP_MOVIE_EXPORT auto
  DemuxSeek(const DemuxContext &demux_ctx, int frame_pos, DemuxFrame *frame) noexcept -> bool;

[[nodiscard]] ILP_MOVIE_EXPORT auto ChannelData(const DemuxFrame &f, Channel ch) noexcept
  -> PixelData<const float>;
[[nodiscard]] ILP_MOVIE_EXPORT auto ChannelData(DemuxFrame *f, Channel ch) noexcept
  -> PixelData<float>;

template<typename PixelT>
[[nodiscard]] ILP_MOVIE_EXPORT constexpr auto Empty(const PixelData<PixelT> &p) noexcept -> bool
{
  return p.data == nullptr || p.count == 0;
}

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
