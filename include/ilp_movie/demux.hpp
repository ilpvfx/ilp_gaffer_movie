#pragma once

#include <cstddef>// std::size_t
#include <cstdint>// std::int64_t, etc.
#include <functional>// std::function
#include <memory>// std::unique_ptr
#include <optional>// std::optional

#include <ilp_movie/ilp_movie_export.hpp>// ILP_MOVIE_EXPORT
#include <ilp_movie/pixel_format.hpp>// ilp_movie::PixelFormat

namespace ilp_movie {

struct DemuxImpl;
using demux_impl_ptr = std::unique_ptr<DemuxImpl, std::function<void(DemuxImpl *)>>;

struct DemuxParams
{
  std::string filename = {};
  std::string filter_graph = "null";// No-op! ("scale=w=0:h=0:out_color_matrix=bt709")
  std::string sws_flags = "flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp";

  // override pixel format?
  // PixelFormat pix_fmt = PixelFormat::kNone;
};

struct DemuxVideoInfo
{
  int width = -1;
  int height = -1;
  double frame_rate = -1.F;

  // One-based frame number range. Seek should return valid frames for the inclusive range:
  // [first, first + count - 1].
  std::int64_t first_frame_nb = -1;
  std::int64_t frame_count = 0;

  PixelFormat pix_fmt = PixelFormat::kNone;
};

struct DemuxContext
{
  DemuxParams params = {};
  DemuxVideoInfo info = {};

  // Private implementation specific data stored as an opaque pointer.
  // The implementation specific data contains low-level resources such as
  // format context, codecs, etc. The resources are automatically freed when this
  // context is destroyed.
  demux_impl_ptr impl = nullptr;
};

struct DemuxFrame
{
  int width = -1;
  int height = -1;
  int64_t frame_nb = -1;
  bool key_frame = false;
  double pixel_aspect_ratio = -1.0;

  PixelFormat pix_fmt = PixelFormat::kNone;

  // NOTE(tohi): If we wanted to support non-floating point formats as well this would need to be
  //             a byte buffer: std::unique_ptr<std::uint8_t> (or std::byte).
  struct
  {
    std::unique_ptr<float[]> data = nullptr;
    std::size_t count = 0;
  } buf = {};
};

[[nodiscard]] ILP_MOVIE_EXPORT auto DemuxMakeContext(const DemuxParams &params/*,
  DemuxFrame *first_frame*/) noexcept -> std::unique_ptr<DemuxContext>;

// Frame number is one-based, i.e. 1 is the first frame of the video stream.
[[nodiscard]] ILP_MOVIE_EXPORT auto
  DemuxSeek(const DemuxContext &demux_ctx, int frame_nb, DemuxFrame *frame) noexcept -> bool;

}// namespace ilp_movie
