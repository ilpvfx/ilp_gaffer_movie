#include <internal/filter_graph.hpp>

#include <array>// std::array
#include <cassert>// assert
#include <cstdio>// std::snprintf

// clang-format off
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/frame.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/avfilter.h>
}
// clang-format on

#include <ilp_movie/log.hpp>
#include <internal/log_utils.hpp>

namespace filter_graph_internal {

class FilterGraphImpl
{
public:
  FilterGraphImpl() = default;
  ~FilterGraphImpl() { _Free(); }

  // Movable.
  FilterGraphImpl(FilterGraphImpl &&rhs) noexcept = default;
  FilterGraphImpl &operator=(FilterGraphImpl &&rhs) noexcept = default;

  // Not copyable.
  FilterGraphImpl(const FilterGraphImpl &rhs) = delete;
  FilterGraphImpl &operator=(const FilterGraphImpl &rhs) = delete;

  [[nodiscard]] auto SetDescription(const FilterGraphDescription &descr) noexcept -> bool
  {
    _Free();

    AVFilterInOut *inputs = nullptr;
    AVFilterInOut *outputs = nullptr;

    const auto exit_func = [&](const bool success) {
      // Always free these before exiting.
      avfilter_inout_free(&inputs);
      avfilter_inout_free(&outputs);
      if (!success) { _Free(); }
      return success;
    };

    _graph = avfilter_graph_alloc();
    inputs = avfilter_inout_alloc();
    outputs = avfilter_inout_alloc();
    if (!(_graph != nullptr && inputs != nullptr && outputs != nullptr)) {
      log_utils_internal::LogAvError("Cannot allocate filter graph", AVERROR(ENOMEM));
      return exit_func(/*success=*/false);
    }

    const AVFilter *buffersrc = avfilter_get_by_name(/*name=*/"buffer");
    const AVFilter *buffersink = avfilter_get_by_name(/*name=*/"buffersink");
    if (!(buffersrc != nullptr && buffersink != nullptr)) {
      log_utils_internal::LogAvError(
        "Cannot find filtering source or sink element", AVERROR_UNKNOWN);// NOLINT
      return exit_func(/*success=*/false);
    }

    // AVFilterContext *filt_src = nullptr;
    // AVFilterContext *filt_out = nullptr;

    if (!(descr.in.width > 0 && descr.in.height > 0
          && descr.in.pix_fmt != AV_PIX_FMT_NONE
          //&& descr.in.sample_aspect_ratio.num > 0 && descr.in.sample_aspect_ratio.den > 0
          && descr.in.time_base.num > 0 && descr.in.time_base.den > 0
          && descr.out.pix_fmt != AV_PIX_FMT_NONE)) {
      log_utils_internal::LogAvError(
        "Bad filter graph in/out parameter(s)", AVERROR(EINVAL));// NOLINT
      return exit_func(/*success=*/false);
    }

    // Buffer video source: the decoded frames from the decoder will be inserted here.
    // clang-format off
    std::array<char, 512> buffersrc_args = {};
    // NOLINTNEXTLINE
    if (const int n = std::snprintf(buffersrc_args.data(), 512 * sizeof(char),
#if 1
          "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
          descr.in.width, descr.in.height, descr.in.pix_fmt,
          descr.in.time_base.num, descr.in.time_base.den,
          descr.in.sample_aspect_ratio.num, descr.in.sample_aspect_ratio.den
#else                 
          "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
          descr.codec_ctx->width, descr.codec_ctx->height, descr.in.pix_fmt,//codec_ctx->pix_fmt,
          descr.in.time_base.num, descr.in.time_base.den,//descr.codec_ctx->time_base.num, descr.codec_ctx->time_base.den,
          descr.codec_ctx->sample_aspect_ratio.num, FFMAX(descr.codec_ctx->sample_aspect_ratio.den, 1),
          enc_ctx->framerate.num, enc_ctx->framerate.den                
#endif                
          ); !(0 < n && n < 512)) {
      ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot create buffer source args\n");
      return exit_func(/*success=*/false);
    }
    // clang-format on

    if (const int ret = avfilter_graph_create_filter(&_buffersrc_ctx,
          buffersrc,
          /*name=*/"in",
          buffersrc_args.data(),
          /*opaque=*/nullptr,
          _graph);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot create buffer source", ret);
      return exit_func(/*success=*/false);
    }

    if (const int ret = avfilter_graph_create_filter(&_buffersink_ctx,
          buffersink,
          /*name=*/"out",
          /*args=*/nullptr,
          /*opaque=*/nullptr,
          _graph);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot create buffer sink", ret);
      return exit_func(/*success=*/false);
    }

#if 0
  AVPixelFormat pix_fmts[] = { descr.out.pix_fmt, AV_PIX_FMT_NONE };// NOLINT
  if (const int ret = av_opt_set_int_list(//NOLINT
        _buffersink_ctx, 
        "pix_fmts", 
        pix_fmts, 
        AV_PIX_FMT_NONE, 
        AV_OPT_SEARCH_CHILDREN);//NOLINT
      ret < 0) {
    log_utils_internal::LogAvError("Cannot set output pixel format", ret);
    exit_func(/*success=*/false);
  }
#else
    if (const int ret = av_opt_set_bin(_buffersink_ctx,
          "pix_fmts",
          reinterpret_cast<const uint8_t *>(&descr.out.pix_fmt),
          sizeof(descr.out.pix_fmt),
          AV_OPT_SEARCH_CHILDREN);// NOLINT
        ret < 0) {
      log_utils_internal::LogAvError("Cannot set output pixel format", ret);
      return exit_func(/*success=*/false);
    }
#endif

    // Set the endpoints for the filter graph. The filter_graph will
    // be linked to the graph described by filters_descr.

    // The buffer source output must be connected to the input pad of
    // the first filter described by filters_descr; since the first
    // filter input label is not specified, it is set to "in" by
    // default.

    outputs->name = av_strdup("in");
    outputs->filter_ctx = _buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = _buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    if (const int ret = avfilter_graph_parse_ptr(
          _graph, descr.filter_descr.c_str(), &inputs, &outputs, /*log_ctx=*/nullptr);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot parse filtergraph", ret);
      return exit_func(/*success=*/false);
    }

    if (const int ret = avfilter_graph_config(_graph, /*log_ctx=*/nullptr); ret < 0) {
      log_utils_internal::LogAvError("Cannot configure filtergraph", ret);
      return exit_func(/*success=*/false);
    }

    if (!descr.sws_flags.empty()) { _graph->scale_sws_opts = av_strdup(descr.sws_flags.c_str()); }

    _filt_frame = av_frame_alloc();
    if (_filt_frame == nullptr) {
      log_utils_internal::LogAvError("Cannot allocate filter frame", AVERROR(ENOMEM));
      return exit_func(/*success=*/false);
    }

    return exit_func(/*success=*/true);
  }

  [[nodiscard]] auto FilterFrames(AVFrame *in_frame,
    const std::function<bool(AVFrame *)> &filter_func) const noexcept -> bool
  {
    if (_graph == nullptr) { return false; }

    // Push the decoded frame into the filter graph.
    int ret = av_buffersrc_add_frame_flags(_buffersrc_ctx, in_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot push frame to the filter graph", ret);
      return false;
    }

    bool keep_going = true;
    while (ret >= 0 && keep_going) {
      // Pull filtered frames from the filter graph.
      ret = av_buffersink_get_frame(_buffersink_ctx, _filt_frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
        // AVERROR(EAGAIN): No frames are available at this point; more input frames must be added
        // to the filter graph to get more output.
        //
        // AVERROR_EOF: There will be no more output frames on this sink.
        ret = 0;
        break;
      }
      if (ret < 0) {
        // An actual error has occured.
        log_utils_internal::LogAvError("Cannot get frame from the filter graph", ret);
        break;
      }
      keep_going = filter_func(_filt_frame);
      av_frame_unref(_filt_frame);
    }
    return ret >= 0 && keep_going;
  }

private:
  void _Free() noexcept
  {
    _buffersrc_ctx = nullptr;
    _buffersink_ctx = nullptr;
    if (_graph != nullptr) {
      avfilter_graph_free(&_graph);
      assert(_graph == nullptr);// NOLINT
    }
    if (_filt_frame != nullptr) {
      av_frame_free(&_filt_frame);
      assert(_filt_frame == nullptr);// NOLINT
    }
  }

  AVFilterGraph *_graph = nullptr;
  AVFilterContext *_buffersrc_ctx = nullptr;
  AVFilterContext *_buffersink_ctx = nullptr;
  AVFrame *_filt_frame = nullptr;
};

// -----------------

FilterGraph::FilterGraph() : _pimpl{ std::make_unique<FilterGraphImpl>() } {}
FilterGraph::~FilterGraph() = default;

auto FilterGraph::SetDescription(const FilterGraphDescription &descr) noexcept -> bool
{
  return Pimpl()->SetDescription(descr);
}

auto FilterGraph::FilterFrames(AVFrame *const in_frame,
  const std::function<bool(AVFrame *)> &filter_func) const noexcept -> bool
{
  return Pimpl()->FilterFrames(in_frame, filter_func);
}

}// namespace filter_graph_internal