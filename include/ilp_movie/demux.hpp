#pragma once

#include <vector>

#include <ilp_movie/ilp_movie_export.hpp>

namespace ilp_movie {

struct DemuxImpl;

struct DemuxContext
{
  const char *filename = nullptr;
  float frame_rate = -1.F;

  // Private implementation specific data stored as an opaque pointer.
  // The implementation specific data contains low-level resources such as
  // format context, codecs, etc.
  DemuxImpl *impl = nullptr;
};

enum class PixelFormat : int {
  kGray,
  kRGB,
  kRGBA,
  kNone,
};

enum class Channel : int {
  kGray,
  kRed,
  kGreen,
  kBlue,
  kAlpha,
};

// Simple span.
template<typename PixelT = const float> struct PixelData
{
  PixelT *data = nullptr;
  size_t count = 0U;// Number of elements.
};

template<typename PixelDataT>
[[nodiscard]] ILP_MOVIE_EXPORT constexpr auto Empty(const PixelDataT &p) noexcept -> bool
{
  return p.data == nullptr || p.count == 0;
}

struct DemuxFrame
{
  int width = -1;
  int height = -1;
  int frame_index = -1;

  // PixelFormat (always float values [0..1])
  // RGB | RGBA

  // Put in a single buffer?
  std::vector<float> buf;
};

[[nodiscard]] ILP_MOVIE_EXPORT auto ChannelData(const DemuxFrame &f, Channel ch) noexcept
  -> PixelData<const float>;
[[nodiscard]] ILP_MOVIE_EXPORT auto ChannelData(DemuxFrame *f, Channel ch) noexcept
  -> PixelData<float>;


[[nodiscard]] ILP_MOVIE_EXPORT auto DemuxInit(DemuxContext *demux_ctx/*,
  DemuxFrame *first_frame*/) noexcept -> bool;

[[nodiscard]] ILP_MOVIE_EXPORT auto
  DemuxSeek(const DemuxContext &demux_ctx, int frame_pos, DemuxFrame *frame) noexcept -> bool;

//[[nodiscard]] ILP_MOVIE_EXPORT auto DemuxFinish() noexcept -> bool;

ILP_MOVIE_EXPORT void DemuxFree(DemuxContext *demux_ctx) noexcept;

}// namespace ilp_movie
