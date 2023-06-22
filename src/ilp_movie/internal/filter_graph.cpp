#include <internal/filter_graph.hpp>

// clang-format off
extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/avfilter.h>
}
// clang-format on

#include <ilp_movie/log.hpp>
#include <internal/log_utils.hpp>

namespace filter_graph_internal {

auto ConfigureVideoFilters(const FilterGraphArgs &args,
  AVFilterGraph **filter_graph,
  AVFilterContext **buffersrc_ctx,
  AVFilterContext **buffersink_ctx
  ) noexcept -> bool
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
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot create buffer sink\n");
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
        graph, args.filter_graph.c_str(), &inputs, &outputs, /*log_ctx=*/nullptr);
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
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot configure filtergraph\n");
    return exit_func(/*success=*/false);
  }

  graph->scale_sws_opts = av_strdup(args.sws_flags.c_str());

  if (!(filter_graph != nullptr && buffersink_ctx != nullptr && buffersink_ctx != nullptr)) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot return filter graph\n");
    return exit_func(/*success=*/false);
  }

  *filter_graph = graph;
  *buffersrc_ctx = filt_src;
  *buffersink_ctx = filt_out;

  return exit_func(/*success=*/true);
}

}// namespace filter_graph_internal