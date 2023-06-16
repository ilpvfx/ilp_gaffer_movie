#include <ilp_movie/mux.hpp>

#include <array>//std::array
#include <cassert>// assert
#include <cstdio>// std::snprintf
#include <cstring>// std::memcpy
#include <memory>// std::unique_ptr
#include <mutex>// std::mutex
#include <optional>// std::optional
#include <sstream>// std::ostringstream

// clang-format off
extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
//#include <libswresample/swresample.h>
}
// clang-format on

#include <ilp_movie/log.hpp>

namespace ilp_movie {

struct MuxImpl
{
  AVDictionary *opt = nullptr;

  AVFormatContext *ofmt_ctx = nullptr;
  AVCodecContext *enc_ctx = nullptr;
  AVPacket *enc_pkt = nullptr;
  AVFrame *enc_frame = nullptr;

  AVFilterGraph *graph = nullptr;
  AVFilterContext *buffersink_ctx = nullptr;
  AVFilterContext *buffersrc_ctx = nullptr;
  // AVFrame *dec_frame = nullptr;

#if 0
  // AVStream *stream = nullptr;
  AVPacket *tmp_pkt = nullptr;
  AVFrame *frame = nullptr;
  AVFrame *tmp_frame = nullptr;
  SwsContext *sws_ctx = nullptr;
#endif

  // const AVOutputFormat *out_format = nullptr;
  // const AVCodec *video_codec = nullptr;
};

}// namespace ilp_movie

namespace {

// Simple RAII class for AVDictionary.
// NOTE(tohi): Not thread-safe.
class Options
{
public:
  Options(const Options &) = default;
  Options &operator=(const Options &) = default;
  Options(Options &&) = default;
  Options &operator=(Options &&) = default;
  ~Options() noexcept
  {
    if (dict != nullptr) {
      av_dict_free(&dict);
      dict = nullptr;
    }
  }

  AVDictionary *dict = nullptr;
};

}// namespace

[[nodiscard]] static auto Count(const Options &opt) noexcept -> int
{
  return opt.dict != nullptr ? av_dict_count(opt.dict) : 0;
}

[[nodiscard]] static auto Empty(const Options &opt) noexcept -> bool { return !(Count(opt) > 0); }

[[nodiscard]] static auto Entries(const Options &opt)
  -> std::vector<std::pair<std::string, std::string>>
{
  if (opt.dict == nullptr) { return {}; }

  // Iterate over all entries.
  std::vector<std::pair<std::string, std::string>> res = {};
  const AVDictionaryEntry *e = nullptr;
  // NOLINTNEXTLINE
  while ((e = av_dict_get(opt.dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
    res.emplace_back(std::make_pair(std::string{ e->key }, std::string{ e->value }));
  }
  return res;
}

[[nodiscard]] static auto SetQScale(AVCodecContext *enc_ctx, const int qscale) -> bool
{
  if (!(enc_ctx != nullptr && qscale >= 0)) { return false; }

  // From ffmpeg source code.
  // NOLINTNEXTLINE
  enc_ctx->flags |= AV_CODEC_FLAG_QSCALE;
  enc_ctx->global_quality = FF_QP2LAMBDA * qscale;
  return true;
}

[[nodiscard]] static auto OpenOutputFile(AVFormatContext **ofmt_ctx,
  AVCodecContext **enc_ctx,
  AVPacket **enc_pkt,
  AVFrame **enc_frame,
  const char *filename,
  const char *enc_name,
  const double fps,
  const int width,
  const int height,
  const std::function<bool(AVDictionary **)> &config_enc) -> bool
{
  const auto exit_func = [&](const bool success) {
    if (!success) {
      if (*ofmt_ctx != nullptr && (*ofmt_ctx)->pb != nullptr
          && !((*ofmt_ctx)->oformat->flags & AVFMT_NOFILE)) {// NOLINT
        avio_closep(&(*ofmt_ctx)->pb);
      }

      av_packet_free(enc_pkt);
      av_frame_free(enc_frame);
      avcodec_free_context(enc_ctx);
      avformat_free_context(*ofmt_ctx);
      *ofmt_ctx = nullptr;
    }
    return success;
  };

  // Allocate the output media context.
  // The output format is automatically guessed based on the filename extension.
  *ofmt_ctx = nullptr;
  if (const int ret = avformat_alloc_output_context2(ofmt_ctx,
        /*oformat=*/nullptr,
        /*format_name=*/nullptr,
        filename);
      ret < 0 || *ofmt_ctx == nullptr) {
    ilp_movie::LogAvError("Could not create output context", ret);
    return exit_func(/*success=*/false);
  }

  // Allocate a stream.
  // NOTE(tohi): c is an unused legacy parameter.
  AVStream *out_stream = avformat_new_stream(*ofmt_ctx, /*c=*/nullptr);
  if (out_stream == nullptr) {
    ilp_movie::LogError("Could not create output stream\n");
    return exit_func(/*success=*/false);
  }
  out_stream->id = static_cast<int>((*ofmt_ctx)->nb_streams - 1U);

  // Find the encoder.
  const AVCodec *encoder = avcodec_find_encoder_by_name(enc_name);
  if (encoder == nullptr) {
    std::ostringstream oss;
    oss << "Could not find encoder '" << enc_name << "'\n";
    ilp_movie::LogError(oss.str().c_str());
    return exit_func(/*success=*/false);
  }
#if 0// Debugging.
  DumpSupportedFramerates(encoder);
  DumpSupportedPixelFormats(encoder);
  DumpSupportedProfiles(encoder);
#endif

  // Allocate an encoder context using the chosen encoder.
  *enc_ctx = avcodec_alloc_context3(encoder);
  if (*enc_ctx == nullptr) {
    ilp_movie::LogError("Could not allocate an encoding context\n");
    return exit_func(/*success=*/false);
  }
  (*enc_ctx)->time_base = av_d2q(1.0 / fps, /*max=*/100000);
  if ((*enc_ctx)->time_base.den <= 0) {
    ilp_movie::LogError("Could not create time base\n");
    return exit_func(/*success=*/false);
  }

  (*enc_ctx)->width = width;
  (*enc_ctx)->height = height;
  (*enc_ctx)->codec_id = encoder->id;
  // impl->enc_ctx->sample_aspect_ratio = ...

  // impl->enc_ctx->gop_size = mux_ctx->h264.gop_size;
  // impl->enc_ctx->max_b_frames = mux_ctx->h264.b_frames;
  // impl->enc_ctx->bit_rate = mux_ctx->h264.bit_rate;
  // impl->enc_ctx->bit_rate_tolerance = mux_ctx->h264.bit_rate_tolerance;
  // impl->enc_ctx->qmin = mux_ctx->h264.quantizer_min;
  // impl->enc_ctx->qmax = mux_ctx->h264.quantizer_max;
#if 0
  // TODO(tohi): need to use filter graph?
  impl->enc_ctx->color_primaries = AVColorPrimaries::AVCOL_PRI_BT709;
  impl->enc_ctx->colorspace = AVColorSpace::AVCOL_SPC_BT709;
  impl->enc_ctx->color_trc = AVColorTransferCharacteristic::AVCOL_TRC_BT709;
  // TODO(tohi): impl->codec_ctx->color_range = AVColorRange::AVCOL_RANGE_
#endif

  // Some formats want stream headers (video, audio) to be separate.
  if ((*ofmt_ctx)->oformat->flags & AVFMT_GLOBALHEADER) {// NOLINT
    (*enc_ctx)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;// NOLINT
  }

  // Codec-specific settings on encoding context.
  Options enc_opt = {};
  if (!config_enc(&enc_opt.dict)) { return exit_func(/*success=*/false); }
  {
    std::ostringstream oss;
    oss << "Encoder options:\n";
    const auto entries = Entries(enc_opt);
    for (auto &&entry : entries) { oss << "('" << entry.first << "', '" << entry.second << "')\n"; }
    ilp_movie::LogInfo(oss.str().c_str());
  }

  // Open the codec.
  if (const int ret = avcodec_open2(*enc_ctx, encoder, &enc_opt.dict); ret < 0) {
    ilp_movie::LogAvError("Could not open video codec", ret);
    return exit_func(/*success=*/false);
  }
  if (!Empty(enc_opt)) {
    std::ostringstream oss;
    oss << "Unused encoder options: ";
    const auto entries = Entries(enc_opt);
    for (auto &&entry : entries) { oss << "('" << entry.first << "', '" << entry.second << "'), "; }
    oss << '\n';
    ilp_movie::LogError(oss.str().c_str());
    return exit_func(/*success=*/false);
  }

  if (const int ret = avcodec_parameters_from_context(out_stream->codecpar, *enc_ctx); ret < 0) {
    ilp_movie::LogAvError("Could not copy encoder parameters to the output stream", ret);
    return exit_func(/*success=*/false);
  }
  out_stream->time_base = (*enc_ctx)->time_base;

  av_dump_format(*ofmt_ctx, /*index=*/0, filename, /*is_output=*/1);

  // Allocate a re-usable packet.
  *enc_pkt = av_packet_alloc();
  if (*enc_pkt == nullptr) {
    ilp_movie::LogAvError("Could not allocate packet for encoding", /*errnum=*/AVERROR(ENOMEM));
    return exit_func(/*success=*/false);
  }

  *enc_frame = av_frame_alloc();
  if (*enc_frame == nullptr) {
    ilp_movie::LogAvError("Could not allocate frame for encoding ", /*errnum=*/AVERROR(ENOMEM));
    return exit_func(/*success=*/false);
  }

  // Open the output file, if needed.
  if (!((*ofmt_ctx)->oformat->flags & AVFMT_NOFILE)) {// NOLINT
    if (const int ret = avio_open(&(*ofmt_ctx)->pb, filename, AVIO_FLAG_WRITE); ret < 0) {
      std::ostringstream oss;
      oss << "Could not open output file '" << filename << "'";
      ilp_movie::LogAvError(oss.str().c_str(), ret);
      return exit_func(/*success=*/false);
    }
  }

  // Write the output file header.
  Options fmt_opt = {};
  if (const int ret = avformat_write_header(*ofmt_ctx, &fmt_opt.dict); ret < 0) {
    ilp_movie::LogAvError("Could not write header", ret);
    return exit_func(/*success=*/false);
  }
  if (!Empty(fmt_opt)) {
    std::ostringstream oss;
    oss << "Unused format options: ";
    const auto entries = Entries(enc_opt);
    for (auto &&entry : entries) { oss << "('" << entry.first << "', '" << entry.second << "'), "; }
    oss << '\n';
    ilp_movie::LogError(oss.str().c_str());
    return exit_func(/*success=*/false);
  }

  return exit_func(/*success=*/true);
}

[[nodiscard]] static auto ConfigureFiltergraph(AVFilterGraph *graph,
  const char *filtergraph,
  AVFilterContext *src_ctx,
  AVFilterContext *sink_ctx) -> bool
{
  assert(graph != nullptr);// NOLINT

  const unsigned int nb_filters = graph->nb_filters;
  AVFilterInOut *inputs = nullptr;
  AVFilterInOut *outputs = nullptr;

  const auto exit_func = [&](const bool success) {
    // Always free these when function scope ends.
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return success;
  };

  inputs = avfilter_inout_alloc();
  outputs = avfilter_inout_alloc();
  if (!(inputs != nullptr && outputs != nullptr)) {
    ilp_movie::LogAvError("Could not allocate filtergraph inputs/outputs", AVERROR(ENOMEM));
    return exit_func(/*success=*/false);
  }

  outputs->name = av_strdup("in");
  outputs->filter_ctx = src_ctx;
  outputs->pad_idx = 0;
  outputs->next = nullptr;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = sink_ctx;
  inputs->pad_idx = 0;
  inputs->next = nullptr;

  if (const int ret =
        avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, /*log_ctx=*/nullptr);
      ret < 0) {
    ilp_movie::LogAvError("Could not parse filtergraph", ret);
    return exit_func(/*success=*/false);
  }

  // Reorder the filters to ensure that inputs of the custom filters are merged first.
  for (unsigned int i = 0; i < graph->nb_filters - nb_filters; ++i) {
    FFSWAP(AVFilterContext *, graph->filters[i], graph->filters[i + nb_filters]);// NOLINT
  }

  if (const int ret = avfilter_graph_config(graph, /*log_ctx=*/nullptr); ret < 0) {
    ilp_movie::LogError("Could not configure filtergraph\n");
    return exit_func(/*success=*/false);
  }

  return exit_func(/*success=*/true);
}

[[nodiscard]] static auto ConfigureVideoFilters(AVFilterGraph *graph,
  AVFilterContext **buffersrc_ctx,
  AVFilterContext **buffersink_ctx,
  const char *filtergraph,
  const char *sws_flags,
  const AVCodecContext *enc_ctx) -> bool
{
  graph->scale_sws_opts = av_strdup(sws_flags);

  // clang-format off
  std::array<char, 512> buffersrc_args = {};
  // NOLINTNEXTLINE
  std::snprintf(buffersrc_args.data(),
                512,
                //"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                enc_ctx->width, enc_ctx->height, AV_PIX_FMT_GBRPF32,
                enc_ctx->time_base.num, enc_ctx->time_base.den,
                enc_ctx->sample_aspect_ratio.num, FFMAX(enc_ctx->sample_aspect_ratio.den, 1));
                //,enc_ctx->framerate.num, enc_ctx->framerate.den);
  // clang-format on

  AVFilterContext *filt_src = nullptr;
  if (const int ret = avfilter_graph_create_filter(&filt_src,
        avfilter_get_by_name(/*name=*/"buffer"),
        /*name=*/"in",
        buffersrc_args.data(),
        /*opaque=*/nullptr,
        graph);
      ret < 0) {
    ilp_movie::LogAvError("Could not create buffer source", ret);
    return false;
  }

  AVFilterContext *filt_out = nullptr;
  if (const int ret = avfilter_graph_create_filter(&filt_out,
        avfilter_get_by_name("buffersink"),
        /*name=*/"out",
        /*args=*/nullptr,
        /*opaque=*/nullptr,
        graph);
      ret < 0) {
    ilp_movie::LogError("Could not create buffer sink\n");
    return false;
  }
  if (const int ret = av_opt_set_bin(filt_out,
        "pix_fmts",
        reinterpret_cast<const uint8_t *>(&enc_ctx->pix_fmt),
        sizeof(enc_ctx->pix_fmt),
        AV_OPT_SEARCH_CHILDREN);// NOLINT
      ret < 0) {
    ilp_movie::LogAvError("Could not set buffer sink pixel format", ret);
    return false;
  }

  if (!ConfigureFiltergraph(graph, filtergraph, filt_src, filt_out)) { return false; }

  *buffersrc_ctx = filt_src;
  *buffersink_ctx = filt_out;

  return true;
}

[[nodiscard]] static auto EncodeWriteFrame(AVFormatContext *ofmt_ctx,
  AVCodecContext *enc_ctx,
  AVPacket *enc_pkt,
  const AVFrame *enc_frame) -> bool
{
  // Encode filtered frame.
  av_packet_unref(enc_pkt);
  if (const int ret = avcodec_send_frame(enc_ctx, enc_frame); ret < 0) {
    ilp_movie::LogAvError("Could not send frame to encoder", ret);
    return false;
  }

  int ret = 0;
  while (ret >= 0) {
    ret = avcodec_receive_packet(enc_ctx, enc_pkt);
    // NOLINTNEXTLINE
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { return true; }

    // Prepare packet for muxing.
    enc_pkt->stream_index = 0;
    av_packet_rescale_ts(enc_pkt, enc_ctx->time_base, ofmt_ctx->streams[0]->time_base);// NOLINT

    // Mux encoded frame.
    ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
    // enc_pkt is now blank. av_interleaved_write_frame() takes ownership of
    // its contents and resets enc_pkt, so that no unreferencing is necessary.
    // This would be different if we used av_write_frame().
  }

  return true;
}

[[nodiscard]] static auto FilterEncodeWriteFrame(AVFormatContext *ofmt_ctx,
  AVCodecContext *enc_ctx,
  AVPacket *enc_pkt,
  AVFrame *enc_frame,
  AVFilterContext *buffersrc_ctx,
  AVFilterContext *buffersink_ctx,
  AVFrame *dec_frame) -> bool
{
  // Push the decoded frame into the filter graph.
  // NOTE(tohi): Resets the frame!
  // if (const int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, dec_frame, /*flags=*/0)) {
  if (const int ret = av_buffersrc_add_frame(buffersrc_ctx, dec_frame)) {
    ilp_movie::LogAvError("Could not push frame to filter graph", ret);
    return false;
  }

  // Pull filtered frames from the filter graph.
  int ret = 0;
  bool w = true;
  while (true) {
    // av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
    ret = av_buffersink_get_frame(buffersink_ctx, enc_frame);
    if (ret < 0) {
      // If no more frames for output             - returns AVERROR(EAGAIN)
      // If flushed and no more frames for output - returns AVERROR_EOF
      //
      // Rewrite return code to 0 to show it as normal procedure completion.
      // NOLINTNEXTLINE
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { ret = 0; }
      break;
    }

    enc_frame->pict_type = AV_PICTURE_TYPE_NONE;
    w = EncodeWriteFrame(ofmt_ctx, enc_ctx, enc_pkt, enc_frame);
    av_frame_unref(enc_frame);
    if (!w) { break; }
  }

  return w && ret >= 0;
}

#if 0
static void LogPacket(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
#if 0
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
#else
  (void)fmt_ctx;
  (void)pkt;
#endif
}
#endif

#if 0
// Encode one video frame and send it to the muxer.
//
// Returns WriteStatus::kEncodingFinished when encoding is finished, WriteStatus::kEncodingOngoing
// otherwise.
//
// Return WriteStatus::kEncodingError if an error occured, in which case muxing should be aborted.
[[nodiscard]] static auto
WriteFrame(AVFormatContext *fmt_ctx, AVCodecContext *c, AVStream *st, AVFrame *frame, AVPacket *pkt)
    -> IlpGafferMovieWriter::WriteStatus {
  char errbuf[AV_ERROR_MAX_STRING_SIZE];
  std::ostringstream oss;

  // Send the frame to the encoder.
  if (const int ret = avcodec_send_frame(c, frame); ret < 0) {
    av_strerror(/*errnum=*/ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
    oss << "[mux] Error sending a frame to the encoder: " << errbuf << "\n";
    MuxLog(oss.str().c_str());
    return IlpGafferMovieWriter::WriteStatus::kEncodingError;
  }

  int ret = 0;
  while (ret >= 0) {
    ret = avcodec_receive_packet(c, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      av_strerror(/*errnum=*/ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      oss << "[mux] Error encoding a frame: " << errbuf << "\n";
      MuxLog(oss.str().c_str());
      return IlpGafferMovieWriter::WriteStatus::kEncodingError;
    }

    // Rescale output packet timestamp values from codec to stream timebase.
    av_packet_rescale_ts(pkt, c->time_base, st->time_base);
    pkt->stream_index = st->index;

    // Write the compressed frame to the media file.
    LogPacket(fmt_ctx, pkt);
    ret = av_interleaved_write_frame(fmt_ctx, pkt);
    // pkt is now blank (av_interleaved_write_frame() takes ownership of
    // its contents and resets pkt), so that no unreferencing is necessary.
    // This would be different if one used av_write_frame().
    if (ret < 0) {
      av_strerror(/*errnum=*/ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      oss << "[mux] Error while writing output packet: " << errbuf << "\n";
      MuxLog(oss.str().c_str());
      return IlpGafferMovieWriter::WriteStatus::kEncodingError;
    }
  }

  return ret == AVERROR_EOF ? IlpGafferMovieWriter::WriteStatus::kEncodingFinished
                            : IlpGafferMovieWriter::WriteStatus::kEncodingOngoing;
}
#endif

namespace ilp_movie {

auto MuxInit(MuxContext *const mux_ctx) -> bool
{
  // Helper function to make sure we always tear down allocated resources
  // before exiting as a result of failure.
  const auto exit_func = [](const bool success, MuxContext *const mux_ctx_arg) {
    if (!success && mux_ctx_arg != nullptr && mux_ctx_arg->impl != nullptr) {
      MuxFree(mux_ctx_arg);
    }
    return success;
  };

  if (mux_ctx == nullptr) {
    LogError("Bad mux context\n");
    return exit_func(/*success=*/false, mux_ctx);
  }
  if (mux_ctx->impl != nullptr) {
    LogError("Found stale mux implementation\n");
    return exit_func(/*success=*/false, mux_ctx);
  }
  if (mux_ctx->filename == nullptr) {
    LogError("Bad filename\n");
    return exit_func(/*success=*/false, mux_ctx);
  }

  // Allocate the implementation specific data.
  // MUST be manually free'd at some later point.
  mux_ctx->impl = new MuxImpl;// NOLINT
  MuxImpl *impl = mux_ctx->impl;

  if (!OpenOutputFile(&impl->ofmt_ctx,
        &impl->enc_ctx,
        &impl->enc_pkt,
        &impl->enc_frame,
        mux_ctx->filename,
        mux_ctx->codec_name,
        mux_ctx->fps,
        mux_ctx->width,
        mux_ctx->height,
        [&](AVDictionary **enc_opt) {
          if (impl->enc_ctx == nullptr) {
            LogError("Encoding context not initialized\n");
            return false;
          }

          const int color_range = av_color_range_from_name(mux_ctx->color_range);
          if (color_range < 0) {
            std::stringstream oss;
            oss << "Could not set color range '" << mux_ctx->color_range << "'";
            LogAvError(oss.str().c_str(), color_range);
            return false;
          }
          impl->enc_ctx->color_range = static_cast<AVColorRange>(color_range);

          const int colorspace = av_color_space_from_name(mux_ctx->colorspace);
          if (colorspace < 0) {
            std::stringstream oss;
            oss << "Could not set colorspace '" << mux_ctx->colorspace << "'";
            LogAvError(oss.str().c_str(), colorspace);
            return false;
          }
          impl->enc_ctx->colorspace = static_cast<AVColorSpace>(colorspace);

          const int color_primaries = av_color_primaries_from_name(mux_ctx->color_primaries);
          if (color_primaries < 0) {
            std::stringstream oss;
            oss << "Could not set color primaries '" << mux_ctx->color_primaries << "'\n";
            LogAvError(oss.str().c_str(), color_primaries);
          }
          impl->enc_ctx->color_primaries = static_cast<AVColorPrimaries>(color_primaries);

          const int color_trc = av_color_transfer_from_name(mux_ctx->color_trc);
          if (color_trc < 0) {
            std::stringstream oss;
            oss << "Could not set color transfer characteristics '" << mux_ctx->color_trc << "'\n";
            LogAvError(oss.str().c_str(), color_trc);
          }
          impl->enc_ctx->color_trc = static_cast<AVColorTransferCharacteristic>(color_trc);

          if (impl->enc_ctx->codec_id == AV_CODEC_ID_H264) {
            const AVPixelFormat pix_fmt = av_get_pix_fmt(mux_ctx->h264.pix_fmt);
            if (pix_fmt == AV_PIX_FMT_NONE) {
              LogError("Could not find pixel format\n");
              return false;
            }
            impl->enc_ctx->pix_fmt = pix_fmt;

            assert(mux_ctx->h264.profile != nullptr);// NOLINT
            assert(mux_ctx->h264.preset != nullptr);// NOLINT
            av_dict_set(enc_opt, "profile", mux_ctx->h264.profile, /*flags=*/0);
            av_dict_set(enc_opt, "preset", mux_ctx->h264.preset, /*flags=*/0);
            av_dict_set(enc_opt, "x264-params", mux_ctx->h264.x264_params, /*flags=*/0);

            if (!(0 <= mux_ctx->h264.crf && mux_ctx->h264.crf <= 51)) {
              LogError("Could not set CRF\n");
              return false;
            }
            av_dict_set_int(enc_opt, "crf", mux_ctx->h264.crf, /*flags=*/0);

            if (mux_ctx->h264.qp >= 0) { av_dict_set_int(enc_opt, "qp", 0, /*flags=*/0); }

            if (mux_ctx->h264.tune != nullptr) {
              av_dict_set(enc_opt, "tune", mux_ctx->h264.tune, /*flags=*/0);
            }
          } else if (impl->enc_ctx->codec_id == AV_CODEC_ID_PRORES) {
            const AVPixelFormat pix_fmt = av_get_pix_fmt(mux_ctx->pro_res.pix_fmt);
            if (pix_fmt == AV_PIX_FMT_NONE) {
              LogError("Could not find pixel format\n");
              return false;
            }
            impl->enc_ctx->pix_fmt = pix_fmt;

            if (!SetQScale(impl->enc_ctx, mux_ctx->pro_res.qscale)) {
              LogError("Could not set qscale\n");
              return false;
            }

            // clang-format off
            assert(mux_ctx->pro_res.profile != nullptr); // NOLINT
            int64_t p = -1;
            if      (std::strcmp(mux_ctx->pro_res.profile, "proxy") == 0)    { p = 0; }
            else if (std::strcmp(mux_ctx->pro_res.profile, "lt") == 0)       { p = 1; }
            else if (std::strcmp(mux_ctx->pro_res.profile, "standard") == 0) { p = 2; }
            else if (std::strcmp(mux_ctx->pro_res.profile, "hq") == 0)       { p = 3; }
            else if (std::strcmp(mux_ctx->pro_res.profile, "4444") == 0)     { p = 4; }
            else if (std::strcmp(mux_ctx->pro_res.profile, "4444xq") == 0)   { p = 5; }
            else {
              LogError("Could not configure ProRes profile\n");
              return false;   
            }
            av_dict_set_int(enc_opt, "profile", p, /*flags=*/0);
            // clang-format on

            av_dict_set(enc_opt, "vendor", mux_ctx->pro_res.vendor, /*flags=*/0);
          } else {
            LogError("Could not configure unsupported encoder\n");
            return false;
          }

          return true;
        })) {
    return exit_func(/*success=*/false, mux_ctx);
  }

  impl->graph = avfilter_graph_alloc();
  if (impl->graph == nullptr) {
    LogAvError("Could not allocate graph", AVERROR(ENOMEM));
    return exit_func(/*success=*/false, mux_ctx);
  }
  if (!ConfigureVideoFilters(impl->graph,
        &impl->buffersrc_ctx,
        &impl->buffersink_ctx,
        mux_ctx->filtergraph,
        mux_ctx->sws_flags,
        impl->enc_ctx)) {
    return exit_func(/*success=*/false, mux_ctx);
  }

  return exit_func(/*success=*/true, mux_ctx);
}

auto MuxWriteFrame(const MuxContext &mux_ctx, const MuxFrame &mux_frame) -> bool
{
  if (mux_ctx.impl == nullptr) {
    LogError("Bad mux context\n");
    return false;
  }
  MuxImpl *impl = mux_ctx.impl;

  // clang-format off
  // TODO(tohi): Any way to check if frame_nb is bad? Must be positive?
  if (!(mux_frame.width == impl->enc_ctx->width && 
        mux_frame.height == impl->enc_ctx->height && 
        mux_frame.r != nullptr &&
        mux_frame.g != nullptr &&
        mux_frame.b != nullptr)) {
    LogError("Bad frame data input\n");
    return false;
  }
  // clang-format on

  // Pretend that a decoder sent us a frame.
  // Create a frame and fill in the pixel data from the given mux frame.
  AVFrame *dec_frame = av_frame_alloc();
  if (dec_frame == nullptr) {
    LogError("Could not allocate decode frame\n");
    return false;
  }
  dec_frame->format = AV_PIX_FMT_GBRPF32;
  dec_frame->width = mux_frame.width;
  dec_frame->height = mux_frame.height;
  dec_frame->pts = static_cast<int64_t>(mux_frame.frame_nb);
  if (const int ret = av_frame_get_buffer(dec_frame, /*align=*/0); ret < 0) {
    LogAvError("Could not allocate decode frame buffer", ret);
    return false;
  }

  const size_t byte_count = static_cast<size_t>(mux_frame.width * mux_frame.height) * sizeof(float);
  std::memcpy(/*__dest=*/dec_frame->data[0], /*__src=*/mux_frame.g, byte_count);
  std::memcpy(/*__dest=*/dec_frame->data[1], /*__src=*/mux_frame.b, byte_count);
  std::memcpy(/*__dest=*/dec_frame->data[2], /*__src=*/mux_frame.r, byte_count);
  dec_frame->pts = static_cast<int64_t>(mux_frame.frame_nb);

  return FilterEncodeWriteFrame(impl->ofmt_ctx,
    impl->enc_ctx,
    impl->enc_pkt,
    impl->enc_frame,
    impl->buffersrc_ctx,
    impl->buffersink_ctx,
    dec_frame);
}

auto MuxFinish(const MuxContext &mux_ctx) -> bool
{
  if (mux_ctx.impl == nullptr) {
    LogError("Bad mux context\n");
    return false;
  }
  MuxImpl *impl = mux_ctx.impl;

  // Flush filter.
  LogInfo("Flushing filter\n");
  if (!FilterEncodeWriteFrame(impl->ofmt_ctx,
        impl->enc_ctx,
        impl->enc_pkt,
        impl->enc_frame,
        impl->buffersrc_ctx,
        impl->buffersink_ctx,
        /*dec_frame=*/nullptr)) {
    LogError("Could not flush filter\n");
    return false;
  }

  // Flush encoder.
  if (impl->enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY) {// NOLINT
    LogInfo("Flushing stream encoder\n");
    if (!EncodeWriteFrame(impl->ofmt_ctx, impl->enc_ctx, impl->enc_pkt, /*enc_frame=*/nullptr)) {
      LogError("Could not flush stream encoder\n");
      return false;
    }
  }

  if (const int ret = av_write_trailer(impl->ofmt_ctx); ret < 0) {
    LogError("Could not write trailer\n");
    return false;
  }

  // Close the output file, if any.
  if (!(impl->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {// NOLINT
    if (const int ret = avio_closep(&impl->ofmt_ctx->pb); ret < 0) {
      LogAvError("Could not close file", ret);
      return false;
    }
  }

  return true;
}

void MuxFree(MuxContext *const mux_ctx)
{
  if (mux_ctx == nullptr || mux_ctx->impl == nullptr) {
    // Nothing to free!
    return;
  }
  MuxImpl *impl = mux_ctx->impl;

  avcodec_free_context(&impl->enc_ctx);
  av_frame_free(&impl->enc_frame);
  // av_frame_free(&impl->dec_frame);
  //  av_packet_free(&impl->tmp_pkt);
  //  sws_freeContext(impl->sws_ctx);
  //(*impl)->sws_ctx = nullptr;
  //  swr_free(&(*impl)->swr_ctx);
  avfilter_graph_free(&impl->graph);
  avformat_free_context(impl->ofmt_ctx);

  // TODO: close file, if not already closed!!!!!!!

  // Free the memory used to store the implementation handles.
  delete impl;// NOLINT
  mux_ctx->impl = nullptr;
}

}// namespace ilp_movie

#if 0
  // Create a new scaling context to match the tmp frame and out frame
  // pixel formats and pixel dimensions.
  impl->sws_ctx = sws_getContext(/*srcW=*/impl->tmp_frame->width,
                                 /*srcH=*/impl->tmp_frame->height,
                                 /*srcFormat=*/(AVPixelFormat)impl->tmp_frame->format,
                                 /*dstW=*/impl->frame->width,
                                 /*dstH=*/impl->frame->height,
                                 /*dstFormat=*/(AVPixelFormat)impl->frame->format,
                                 /*flags=*/SWS_BICUBIC,
                                 /*srcFilter=*/nullptr,
                                 /*dstFilter=*/nullptr,
                                 /*param=*/nullptr);
  if (impl->sws_ctx == nullptr) {
    MuxLog("[mux] Could not initialize the frame conversion context\n");
    return false;
  }
#endif

#if 0
  // AVCodecID codec_id = ;
  {
    std::ostringstream oss;
    oss << "Codecs:\n";
    oss << avcodec_get_name(AV_CODEC_ID_PRORES) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_H264) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_MJPEG) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_MJPEGB) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_MSMPEG4V3) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_JPEG2000) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_PNG) << "\n";
    oss << avcodec_get_name(AV_CODEC_ID_WRAPPED_AVFRAME) << "\n";
    MuxLog(oss.str().c_str());
  }
#endif
