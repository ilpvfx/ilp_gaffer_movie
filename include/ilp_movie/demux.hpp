#pragma once

#include <cstddef>// std::size_t
#include <functional>// std::function
#include <memory>// std::unique_ptr

#include <ilp_movie/ilp_movie_export.hpp>
#include <ilp_movie/pixel_format.hpp>

namespace ilp_movie {

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

struct DemuxFrame
{
  int width = -1;
  int height = -1;
  int frame_index = -1;
  bool key_frame = false;
  double pixel_aspect_ratio = -1.0;

  PixelFormat pix_fmt = PixelFormat::kNone;

  // NOTE(tohi): If we wanted to support non-floating point formats as well this would need to be
  //             a byte buffer: std::unique_ptr<std::uint8_t> (or std::byte).
  struct {
    std::unique_ptr<float[]> data = nullptr;
    std::size_t count = 0;
  } buf = {};
};

[[nodiscard]] ILP_MOVIE_EXPORT auto MakeDemuxContext(const DemuxParams &params/*,
  DemuxFrame *first_frame*/) noexcept -> std::unique_ptr<DemuxContext>;

[[nodiscard]] ILP_MOVIE_EXPORT auto
  DemuxSeek(const DemuxContext &demux_ctx, int frame_pos, DemuxFrame *frame) noexcept -> bool;

}// namespace ilp_movie
