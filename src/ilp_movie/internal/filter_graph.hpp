#pragma once

#include <functional>// std::function
#include <memory>// std::unique_ptr
#include <string>// std::string

// clang-format off
extern "C" {
#include <libavutil/rational.h>// AVRational
#include <libavutil/pixfmt.h>// AVPixelFormat
}
// clang-format on

#include <ilp_movie/ilp_movie_export.hpp>

// Forward declarations.
struct AVFrame;

namespace filter_graph_internal {

struct FilterGraphArgs
{
  // const AVCodecContext *codec_ctx = nullptr;
  std::string filter_descr = "null";// Pass-through
  std::string sws_flags = "";

  struct
  {
    // Typically from codec context.
    int width = -1;
    int height = -1;
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    AVRational sample_aspect_ratio = { /*.num=*/0, /*.den=*/0 };

    // Typically from stream.
    AVRational time_base = { /*.num=*/0, /*.den=*/0 };
  } in;

  struct
  {
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
  } out;
};

class FilterGraphImpl;
class ILP_MOVIE_NO_EXPORT FilterGraph
{
public:
  explicit FilterGraph(const FilterGraphArgs &args);
  ~FilterGraph();

  // Movable.
  FilterGraph(FilterGraph &&rhs) noexcept = default;
  FilterGraph &operator=(FilterGraph &&rhs) noexcept = default;

  // Not copyable.
  FilterGraph(const FilterGraph &rhs) = delete;
  FilterGraph &operator=(const FilterGraph &rhs) = delete;

  [[nodiscard]] auto FilterFrame(AVFrame *in_frame,
    const std::function<void(AVFrame *)> &filter_func) const noexcept -> bool;

private:
  const FilterGraphImpl *Pimpl() const { return _pimpl.get(); }
  FilterGraphImpl *Pimpl() { return _pimpl.get(); }

  std::unique_ptr<FilterGraphImpl> _pimpl;
};

#if 0
[[nodiscard]] ILP_MOVIE_NO_EXPORT auto ConfigureVideoFilters(const FilterGraphArgs &args,
  AVFilterGraph **graph,
  AVFilterContext **buffersrc_ctx,
  AVFilterContext **buffersink_ctx) noexcept -> bool;
#endif

}// namespace filter_graph_internal
