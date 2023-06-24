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

struct DemuxFrame
{
  int width = -1;
  int height = -1;
  int frame_index = -1;
  std::vector<float> r;
  std::vector<float> g;
  std::vector<float> b;
};

[[nodiscard]] ILP_MOVIE_EXPORT auto DemuxInit(DemuxContext *demux_ctx/*,
  DemuxFrame *first_frame*/) noexcept -> bool;

[[nodiscard]] ILP_MOVIE_EXPORT auto
  DemuxSeek(const DemuxContext &demux_ctx, int frame_pos, DemuxFrame *frame) noexcept -> bool;

//[[nodiscard]] ILP_MOVIE_EXPORT auto DemuxFinish() noexcept -> bool;

ILP_MOVIE_EXPORT void DemuxFree(DemuxContext *demux_ctx) noexcept;

}// namespace ilp_movie
