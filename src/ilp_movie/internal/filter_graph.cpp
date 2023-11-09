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
  explicit FilterGraphImpl(const FilterGraphArgs &args);
  ~FilterGraphImpl();

  // Movable.
  FilterGraphImpl(FilterGraphImpl &&rhs) noexcept = default;
  FilterGraphImpl &operator=(FilterGraphImpl &&rhs) noexcept = default;

  // Not copyable.
  FilterGraphImpl(const FilterGraphImpl &rhs) = delete;
  FilterGraphImpl &operator=(const FilterGraphImpl &rhs) = delete;

  [[nodiscard]] auto FilterFrame(AVFrame *in_frame,
    const std::function<void(AVFrame *)> &filter_func) const noexcept -> bool;

private:
  void _Free() noexcept;

  AVFilterGraph *_graph = nullptr;
  AVFilterContext *_buffersrc_ctx = nullptr;
  AVFilterContext *_buffersink_ctx = nullptr;
  AVFrame *_filt_frame = nullptr;
};

FilterGraphImpl::FilterGraphImpl(const FilterGraphArgs &args)
{
  AVFilterInOut *inputs = nullptr;
  AVFilterInOut *outputs = nullptr;

  const auto exit_func = [&](const bool success) {
    // Always free these when function scope ends.
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (!success) {
      _Free();
      throw std::invalid_argument{ "FilterGraphImpl::FilterGraphImpl" };// msg!?!?!?
    }
  };

  _graph = avfilter_graph_alloc();
  inputs = avfilter_inout_alloc();
  outputs = avfilter_inout_alloc();
  if (!(_graph != nullptr && inputs != nullptr && outputs != nullptr)) {
    log_utils_internal::LogAvError("Cannot allocate filter graph", AVERROR(ENOMEM));
    exit_func(/*success=*/false);
  }

  const AVFilter *buffersrc = avfilter_get_by_name(/*name=*/"buffer");
  const AVFilter *buffersink = avfilter_get_by_name(/*name=*/"buffersink");
  if (!(buffersrc != nullptr && buffersink != nullptr)) {
    log_utils_internal::LogAvError(
      "Cannot find filtering source or sink element", AVERROR_UNKNOWN);// NOLINT
    exit_func(/*success=*/false);
  }

  // AVFilterContext *filt_src = nullptr;
  // AVFilterContext *filt_out = nullptr;

  if (!(args.in.width > 0 && args.in.height > 0
        && args.in.pix_fmt != AV_PIX_FMT_NONE
        //&& args.in.sample_aspect_ratio.num > 0 && args.in.sample_aspect_ratio.den > 0
        && args.in.time_base.num > 0 && args.in.time_base.den > 0
        && args.out.pix_fmt != AV_PIX_FMT_NONE)) {
    log_utils_internal::LogAvError(
      "Bad filter graph in/out parameter(s)", AVERROR(EINVAL));// NOLINT
    exit_func(/*success=*/false);
  }

  // Buffer video source: the decoded frames from the decoder will be inserted here.
  // clang-format off
  std::array<char, 512> buffersrc_args = {};
  // NOLINTNEXTLINE
  if (const int n = std::snprintf(buffersrc_args.data(),
                512 * sizeof(char),
#if 1
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                args.in.width, args.in.height, args.in.pix_fmt,
                args.in.time_base.num, args.in.time_base.den,
                args.in.sample_aspect_ratio.num, args.in.sample_aspect_ratio.den
#else                 
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                args.codec_ctx->width, args.codec_ctx->height, args.in.pix_fmt,//codec_ctx->pix_fmt,
                args.in.time_base.num, args.in.time_base.den,//args.codec_ctx->time_base.num, args.codec_ctx->time_base.den,
                args.codec_ctx->sample_aspect_ratio.num, FFMAX(args.codec_ctx->sample_aspect_ratio.den, 1),
                enc_ctx->framerate.num, enc_ctx->framerate.den                
#endif                
                ); !(0 < n && n < 512)) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot create buffer source args\n");
    exit_func(/*success=*/false);
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
    exit_func(/*success=*/false);
  }

  if (const int ret = avfilter_graph_create_filter(&_buffersink_ctx,
        buffersink,
        /*name=*/"out",
        /*args=*/nullptr,
        /*opaque=*/nullptr,
        _graph);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot create buffer sink", ret);
    exit_func(/*success=*/false);
  }

#if 0
  AVPixelFormat pix_fmts[] = { args.out.pix_fmt, AV_PIX_FMT_NONE };// NOLINT
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
        reinterpret_cast<const uint8_t *>(&args.out.pix_fmt),
        sizeof(args.out.pix_fmt),
        AV_OPT_SEARCH_CHILDREN);// NOLINT
      ret < 0) {
    log_utils_internal::LogAvError("Cannot set output pixel format", ret);
    exit_func(/*success=*/false);
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
        _graph, args.filter_descr.c_str(), &inputs, &outputs, /*log_ctx=*/nullptr);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot parse filtergraph", ret);
    exit_func(/*success=*/false);
  }

  if (const int ret = avfilter_graph_config(_graph, /*log_ctx=*/nullptr); ret < 0) {
    log_utils_internal::LogAvError("Cannot configure filtergraph", ret);
    exit_func(/*success=*/false);
  }

  if (!args.sws_flags.empty()) { _graph->scale_sws_opts = av_strdup(args.sws_flags.c_str()); }

  _filt_frame = av_frame_alloc();
  if (_filt_frame == nullptr) {
    log_utils_internal::LogAvError("Cannot allocate filter frame", AVERROR(ENOMEM));
    exit_func(/*success=*/false);
  }

  // if (!(filter_graph != nullptr && buffersink_ctx != nullptr && buffersink_ctx != nullptr)) {
  //   ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot return filter graph\n");
  //   exit_func(/*success=*/false);
  // }

  // *filter_graph = graph;
  // *buffersrc_ctx = filt_src;
  // *buffersink_ctx = filt_out;

  exit_func(/*success=*/true);
}

FilterGraphImpl::~FilterGraphImpl() { _Free(); }

auto FilterGraphImpl::FilterFrame(AVFrame *const in_frame,
  const std::function<void(AVFrame *)> &filter_func) const noexcept -> bool
{
  // Push the decoded frame into the filter graph.
  if (const int ret =
        av_buffersrc_add_frame_flags(_buffersrc_ctx, in_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot push frame to the filter graph", ret);
    return false;
  }

  // int frame_count = 0;
  int ret = 0;
  // pull filtered frames from the filter graph.
  while (true) {
    ret = av_buffersink_get_frame(_buffersink_ctx, _filt_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
      break;
    }
    if (ret < 0) { break; }
    assert(ret >= 0);// NOLINT
    filter_func(_filt_frame);
    av_frame_unref(_filt_frame);

    // if (ret >= 0) {
    //   ++frame_count;
    //   assert(frame_count == 1);// NOLINT
    //   // Keep going, interesting to see how many frames we get (hopefully only one).
    // } else {
    //   // This is the only way for the loop to end.
    //   break;
    // }
  }
  assert(ret < 0);// NOLINT

  // AVERROR(EAGAIN): No frames are available at this point; more input frames must be added
  // to the filter graph to get more output.
  //
  // AVERROR_EOF: There will be no more output frames on this sink.
  //
  // These are not really errors, at least not if we already managed to read one frame.
  const bool err = ret != AVERROR(EAGAIN) && ret != AVERROR_EOF;// NOLINT
  if (err) {
    // An actual error has occured.
    log_utils_internal::LogAvError("Cannot get frame from the filter graph", ret);
  }

  // We are expecting to get exactly one frame.
  return !err;// && frame_count == 1;
}

void FilterGraphImpl::_Free() noexcept
{
  if (_graph != nullptr) { avfilter_graph_free(&_graph); }
  if (_filt_frame != nullptr) { av_frame_free(&_filt_frame); }
}

// -----------------

FilterGraph::FilterGraph(const FilterGraphArgs &args)
  : _pimpl{ std::make_unique<FilterGraphImpl>(args) }
{}

FilterGraph::~FilterGraph() = default;

auto FilterGraph::FilterFrame(AVFrame *const in_frame,
  const std::function<void(AVFrame *)> &filter_func) const noexcept -> bool
{
  return Pimpl()->FilterFrame(in_frame, filter_func);
}

#if 0
auto ConfigureVideoFilters(const FilterGraphArgs &args,
  AVFilterGraph **filter_graph,
  AVFilterContext **buffersrc_ctx,
  AVFilterContext **buffersink_ctx) noexcept -> bool
{
  AVFilterGraph *graph = nullptr;
  AVFilterInOut *inputs = nullptr;
  AVFilterInOut *outputs = nullptr;

  const auto exit_func = [&](const bool success) {
    // Always free these when function scope ends.
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return success;
  };

  graph = avfilter_graph_alloc();
  inputs = avfilter_inout_alloc();
  outputs = avfilter_inout_alloc();
  if (!(graph != nullptr && inputs != nullptr && outputs != nullptr)) {
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

  if (args.codec_ctx == nullptr) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Missing codec context\n");
    return exit_func(/*success=*/false);
  }

  AVFilterContext *filt_src = nullptr;
  AVFilterContext *filt_out = nullptr;

  // Buffer video source: the decoded frames from the decoder will be inserted here.
  // clang-format off
  std::array<char, 512> buffersrc_args = {};
  // NOLINTNEXTLINE
  std::snprintf(buffersrc_args.data(),
                512 * sizeof(char),
                //"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                args.codec_ctx->width, args.codec_ctx->height, args.in.pix_fmt,//codec_ctx->pix_fmt,
                args.in.time_base.num, args.in.time_base.den,//args.codec_ctx->time_base.num, args.codec_ctx->time_base.den,
                args.codec_ctx->sample_aspect_ratio.num, FFMAX(args.codec_ctx->sample_aspect_ratio.den, 1));
                //,enc_ctx->framerate.num, enc_ctx->framerate.den);
  // clang-format on
  if (const int ret = avfilter_graph_create_filter(&filt_src,
        buffersrc,
        /*name=*/"in",
        buffersrc_args.data(),
        /*opaque=*/nullptr,
        graph);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot create buffer source", ret);
    return exit_func(/*success=*/false);
  }

  if (const int ret = avfilter_graph_create_filter(&filt_out,
        buffersink,
        /*name=*/"out",
        /*args=*/nullptr,
        /*opaque=*/nullptr,
        graph);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot create buffer sink", ret);
    return exit_func(/*success=*/false);
  }
  if (const int ret = av_opt_set_bin(filt_out,
        "pix_fmts",
        reinterpret_cast<const uint8_t *>(&args.out.pix_fmt),
        sizeof(args.out.pix_fmt),
        AV_OPT_SEARCH_CHILDREN);// NOLINT
      ret < 0) {
    log_utils_internal::LogAvError("Cannot set output pixel format", ret);
    return exit_func(/*success=*/false);
  }

  outputs->name = av_strdup("in");
  outputs->filter_ctx = filt_src;
  outputs->pad_idx = 0;
  outputs->next = nullptr;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = filt_out;
  inputs->pad_idx = 0;
  inputs->next = nullptr;

  if (const int ret = avfilter_graph_parse_ptr(
        graph, args.filter_descr.c_str(), &inputs, &outputs, /*log_ctx=*/nullptr);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot parse filtergraph", ret);
    return exit_func(/*success=*/false);
  }

#if 0
  // Reorder the filters to ensure that inputs of the custom filters are merged first.
  const unsigned int nb_filters = graph->nb_filters;
  for (unsigned int i = 0; i < graph->nb_filters - nb_filters; ++i) {
    FFSWAP(AVFilterContext *, graph->filters[i], graph->filters[i + nb_filters]);// NOLINT
  }
#endif

  if (const int ret = avfilter_graph_config(graph, /*log_ctx=*/nullptr); ret < 0) {
    log_utils_internal::LogAvError("Cannot configure filtergraph", ret);
    return exit_func(/*success=*/false);
  }

  if (!args.sws_flags.empty()) { graph->scale_sws_opts = av_strdup(args.sws_flags.c_str()); }

  if (!(filter_graph != nullptr && buffersink_ctx != nullptr && buffersink_ctx != nullptr)) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot return filter graph\n");
    return exit_func(/*success=*/false);
  }

  *filter_graph = graph;
  *buffersrc_ctx = filt_src;
  *buffersink_ctx = filt_out;

  return exit_func(/*success=*/true);
}
#endif

}// namespace filter_graph_internal