#pragma once

#include <string>

// clang-format off
extern "C" {
#include <libavutil/rational.h>
#include <libavutil/pixfmt.h>
}
// clang-format on

#include <ilp_movie/ilp_movie_export.hpp>

// Forward declarations.
struct AVFilterGraph;
struct AVFilterContext;
struct AVCodecContext;

namespace filter_graph_internal {

struct FilterGraphArgs
{
  const AVCodecContext *codec_ctx = nullptr;
  std::string filter_graph = "null";// Pass-through
  std::string sws_flags = "";

  struct
  {
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    AVRational time_base = {};
#if 0
    int width = -1;
    int height = -1;
    AVRational timebase;// fmt_ctx->streams[video_stream_index]->time_base
    AVRational sample_aspect_ratio;
#endif
  } in;

  struct
  {
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
  } out;
};

[[nodiscard]] ILP_MOVIE_NO_EXPORT auto ConfigureVideoFilters(const FilterGraphArgs &args,
  AVFilterGraph **graph,
  AVFilterContext **buffersrc_ctx,
  AVFilterContext **buffersink_ctx) noexcept -> bool;

}// namespace filter_graph_internal
