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

struct FilterGraphDescription
{
  std::string filter_descr = "null";// Pass-through (for video), use "anull" for audio.

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
  FilterGraph();
  ~FilterGraph();

  // Movable.
  FilterGraph(FilterGraph &&rhs) noexcept = default;
  FilterGraph &operator=(FilterGraph &&rhs) noexcept = default;

  // Not copyable.
  FilterGraph(const FilterGraph &rhs) = delete;
  FilterGraph &operator=(const FilterGraph &rhs) = delete;

  [[nodiscard]] auto SetDescription(const FilterGraphDescription &descr) noexcept -> bool;

  [[nodiscard]] auto FilterFrames(AVFrame *in_frame,
    const std::function<bool(AVFrame *)> &filter_func) const noexcept -> bool;

private:
  const FilterGraphImpl *Pimpl() const { return _pimpl.get(); }
  FilterGraphImpl *Pimpl() { return _pimpl.get(); }

  std::unique_ptr<FilterGraphImpl> _pimpl;
};

}// namespace filter_graph_internal
