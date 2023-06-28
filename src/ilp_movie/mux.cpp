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
#include <internal/dict_utils.hpp>
#include <internal/filter_graph.hpp>
#include <internal/log_utils.hpp>

namespace ilp_movie {

struct MuxImpl
{
  // AVDictionary *opt = nullptr;

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
};

}// namespace ilp_movie

[[nodiscard]] static auto OpenOutputFile(const char *filename,
  const char *enc_name,
  const double fps,
  const int width,
  const int height,
  const std::function<bool(AVCodecContext *, AVDictionary **)> &config_enc,
  AVFormatContext **ofmt_ctx,
  AVCodecContext **enc_ctx) noexcept -> bool
{
  // Allocate the output media context.
  // The output format is automatically guessed based on the filename extension.
  AVFormatContext *fmt_ctx = nullptr;
  if (const int ret = avformat_alloc_output_context2(&fmt_ctx,
        /*oformat=*/nullptr,
        /*format_name=*/nullptr,
        filename);
      ret < 0 || fmt_ctx == nullptr) {
    log_utils_internal::LogAvError("Cannot create output context", ret);
    return false;
  }

  // Allocate a stream.
  // NOTE(tohi): c is an unused legacy parameter.
  AVStream *out_stream = avformat_new_stream(fmt_ctx, /*c=*/nullptr);
  if (out_stream == nullptr) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot create output stream\n");
    return false;
  }
  out_stream->id = static_cast<int>(fmt_ctx->nb_streams - 1U);

  // Find the encoder.
  const AVCodec *encoder = avcodec_find_encoder_by_name(enc_name);
  if (encoder == nullptr) {
    std::ostringstream oss;
    oss << "Cannot find encoder '" << enc_name << "'\n";
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, oss.str().c_str());
    return false;
  }

  // Allocate an encoder context using the chosen encoder.
  AVCodecContext *codec_ctx = avcodec_alloc_context3(encoder);
  if (codec_ctx == nullptr) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot allocate an encoding context\n");
    return false;
  }
  codec_ctx->time_base = av_d2q(1.0 / fps, /*max=*/100000);
  if (codec_ctx->time_base.den <= 0) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot create time base\n");
    return false;
  }

  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->codec_id = encoder->id;
  // impl->enc_ctx->sample_aspect_ratio = ...

#if 0
  // TODO(tohi): need to use filter graph?
  impl->enc_ctx->color_primaries = AVColorPrimaries::AVCOL_PRI_BT709;
  impl->enc_ctx->colorspace = AVColorSpace::AVCOL_SPC_BT709;
  impl->enc_ctx->color_trc = AVColorTransferCharacteristic::AVCOL_TRC_BT709;
#endif

  // Some formats want stream headers (video, audio) to be separate.
  if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {// NOLINT
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;// NOLINT
  }

  // Codec-specific settings on encoding context.
  dict_utils_internal::Options enc_opt = {};
  if (!config_enc(codec_ctx, &(enc_opt.dict))) { return false; }

  {
    std::ostringstream oss;
    oss << "Encoder options:\n";
    const auto entries = Entries(enc_opt);
    for (auto &&entry : entries) { oss << "('" << entry.first << "', '" << entry.second << "')\n"; }
    ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
  }

  // Open the codec.
  if (const int ret = avcodec_open2(codec_ctx, encoder, &enc_opt.dict); ret < 0) {
    log_utils_internal::LogAvError("Cannot open video codec", ret);
    return false;
  }
  if (!Empty(enc_opt)) {
    std::ostringstream oss;
    oss << "Unused encoder options: ";
    const auto entries = Entries(enc_opt);
    for (auto &&entry : entries) { oss << "('" << entry.first << "', '" << entry.second << "'), "; }
    oss << '\n';
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, oss.str().c_str());
    return false;
  }

  if (const int ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx); ret < 0) {
    log_utils_internal::LogAvError("Cannot copy encoder parameters to the output stream", ret);
    return false;
  }
  out_stream->time_base = codec_ctx->time_base;

  // Open the output file, if needed.
  if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {// NOLINT
    if (const int ret = avio_open(&(fmt_ctx)->pb, filename, AVIO_FLAG_WRITE); ret < 0) {
      std::ostringstream oss;
      oss << "Cannot open output file '" << filename << "'";
      log_utils_internal::LogAvError(oss.str().c_str(), ret);
      return false;
    }
  }

  // Write the output file header.
  dict_utils_internal::Options fmt_opt = {};
  if (const int ret = avformat_write_header(fmt_ctx, &fmt_opt.dict); ret < 0) {
    log_utils_internal::LogAvError("Cannot write header", ret);
    return false;
  }
  if (!dict_utils_internal::Empty(fmt_opt)) {
    std::ostringstream oss;
    oss << "Unused format options: ";
    const auto entries = dict_utils_internal::Entries(enc_opt);
    for (auto &&entry : entries) { oss << "('" << entry.first << "', '" << entry.second << "'), "; }
    oss << '\n';
    ilp_movie::LogMsg(ilp_movie::LogLevel::kError, oss.str().c_str());
    return false;
  }

  *ofmt_ctx = fmt_ctx;
  *enc_ctx = codec_ctx;

  return true;
}

[[nodiscard]] static auto SetQScale(AVCodecContext *enc_ctx, const int qscale) noexcept -> bool
{
  if (!(enc_ctx != nullptr && qscale >= 0)) { return false; }

  // From ffmpeg source code.
  // NOLINTNEXTLINE
  enc_ctx->flags |= AV_CODEC_FLAG_QSCALE;
  enc_ctx->global_quality = FF_QP2LAMBDA * qscale;
  return true;
}

[[nodiscard]] static auto EncodeWriteFrame(AVFormatContext *ofmt_ctx,
  AVCodecContext *enc_ctx,
  AVPacket *enc_pkt,
  const AVFrame *enc_frame) noexcept -> bool
{
  // Encode filtered frame.
  av_packet_unref(enc_pkt);
  if (const int ret = avcodec_send_frame(enc_ctx, enc_frame); ret < 0) {
    log_utils_internal::LogAvError("Cannot send frame to encoder", ret);
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
  AVFrame *dec_frame) noexcept -> bool
{
  // Push the decoded frame into the filter graph.
  // NOTE(tohi): Resets the frame!
  // if (const int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, dec_frame, /*flags=*/0)) {
  if (const int ret = av_buffersrc_add_frame(buffersrc_ctx, dec_frame)) {
    log_utils_internal::LogAvError("Could not push frame to filter graph", ret);
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
static void LogPacket(const AVFormatContext *fmt_ctx, const AVPacket *pkt) noexcept {
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

namespace ilp_movie {

auto MakeMuxContext(const MuxParams &params) noexcept -> std::unique_ptr<MuxContext>
{
  if (params.filename == nullptr) {
    LogMsg(LogLevel::kError, "Empty filename\n");
    return nullptr;
  }

  // Allocate the implementation specific data. The deleter will be invoked if we leave this
  // function before moving the implementation to the context.
  auto impl = mux_impl_ptr(new MuxImpl(), [](MuxImpl *impl_ptr) noexcept {
    LogMsg(LogLevel::kInfo, "Free mux implementation\n");
    assert(impl_ptr != nullptr);// NOLINT

    avfilter_graph_free(&(impl_ptr->graph));
    avcodec_free_context(&(impl_ptr->enc_ctx));
    av_frame_free(&(impl_ptr->enc_frame));
    av_packet_free(&(impl_ptr->enc_pkt));

    if (impl_ptr->ofmt_ctx != nullptr) {
      if (!(impl_ptr->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {// NOLINT
        if (impl_ptr->ofmt_ctx->pb != nullptr) {
          if (const int ret = avio_closep(&(impl_ptr->ofmt_ctx->pb)); ret < 0) {
            log_utils_internal::LogAvError("Cannot close file", ret);
            // return false!?
          }
        }
      }
    }

    avformat_free_context(impl_ptr->ofmt_ctx);
    delete impl_ptr;// NOLINT
  });

  // Allocate packets and frames.
  impl->enc_pkt = av_packet_alloc();
  impl->enc_frame = av_frame_alloc();
  if (!(impl->enc_pkt != nullptr && impl->enc_frame != nullptr)) {
    LogMsg(LogLevel::kError, "Cannot allocate frames/packets\n");
    return nullptr;
  }

  if (!OpenOutputFile(
        params.filename,
        params.codec_name,
        params.fps,
        params.width,
        params.height,
        [&](AVCodecContext *codec_ctx, AVDictionary **enc_opt) {
          if (codec_ctx == nullptr) {
            LogMsg(LogLevel::kError, "Encoding context not initialized\n");
            return false;
          }

          const int color_range = av_color_range_from_name(params.mp4_metadata.color_range);
          if (color_range < 0) {
            std::stringstream oss;
            oss << "Could not set color range '" << params.mp4_metadata.color_range << "'";
            log_utils_internal::LogAvError(oss.str().c_str(), color_range);
            return false;
          }
          codec_ctx->color_range = static_cast<AVColorRange>(color_range);

          const int colorspace = av_color_space_from_name(params.mp4_metadata.colorspace);
          if (colorspace < 0) {
            std::stringstream oss;
            oss << "Could not set colorspace '" << params.mp4_metadata.colorspace << "'";
            log_utils_internal::LogAvError(oss.str().c_str(), colorspace);
            return false;
          }
          codec_ctx->colorspace = static_cast<AVColorSpace>(colorspace);

          const int color_primaries =
            av_color_primaries_from_name(params.mp4_metadata.color_primaries);
          if (color_primaries < 0) {
            std::stringstream oss;
            oss << "Could not set color primaries '" << params.mp4_metadata.color_primaries
                << "'\n";
            log_utils_internal::LogAvError(oss.str().c_str(), color_primaries);
          }
          codec_ctx->color_primaries = static_cast<AVColorPrimaries>(color_primaries);

          const int color_trc = av_color_transfer_from_name(params.mp4_metadata.color_trc);
          if (color_trc < 0) {
            std::stringstream oss;
            oss << "Could not set color transfer characteristics '" << params.mp4_metadata.color_trc
                << "'\n";
            log_utils_internal::LogAvError(oss.str().c_str(), color_trc);
          }
          codec_ctx->color_trc = static_cast<AVColorTransferCharacteristic>(color_trc);

          if (codec_ctx->codec_id == AV_CODEC_ID_H264) {
            const AVPixelFormat pix_fmt = av_get_pix_fmt(params.h264.cfg.pix_fmt);
            if (pix_fmt == AV_PIX_FMT_NONE) {
              LogMsg(LogLevel::kError, "Could not find pixel format\n");
              return false;
            }
            codec_ctx->pix_fmt = pix_fmt;

            if (!(0 <= params.h264.cfg.crf && params.h264.cfg.crf <= 51)) {
              LogMsg(LogLevel::kError, "Bad CRF (must be in [0..51])\n");
              return false;
            }

            av_dict_set(enc_opt, "preset", params.h264.cfg.preset, /*flags=*/0);
            av_dict_set_int(enc_opt, "crf", params.h264.cfg.crf, /*flags=*/0);
            av_dict_set(enc_opt, "x264-params", params.h264.cfg.x264_params, /*flags=*/0);
            if (params.h264.cfg.tune != nullptr) {
              av_dict_set(enc_opt, "tune", params.h264.cfg.tune, /*flags=*/0);
            }
          } else if (codec_ctx->codec_id == AV_CODEC_ID_PRORES) {
            const AVPixelFormat pix_fmt = av_get_pix_fmt(params.pro_res.cfg.pix_fmt);
            if (pix_fmt == AV_PIX_FMT_NONE) {
              LogMsg(LogLevel::kError, "Could not find pixel format\n");
              return false;
            }
            codec_ctx->pix_fmt = pix_fmt;

            if (!SetQScale(codec_ctx, params.pro_res.cfg.qscale)) {
              LogMsg(LogLevel::kError, "Could not set qscale\n");
              return false;
            }

            av_dict_set_int(
              enc_opt, "profile", static_cast<int64_t>(params.pro_res.cfg.profile), /*flags=*/0);
            av_dict_set(enc_opt, "vendor", params.pro_res.cfg.vendor, /*flags=*/0);
          } else {
            LogMsg(LogLevel::kError, "Could not configure unsupported encoder\n");
            return false;
          }

          return true;
        },
        &(impl->ofmt_ctx),
        &(impl->enc_ctx))) {
    return nullptr;
  }

  av_dump_format(impl->ofmt_ctx, /*index=*/0, params.filename, /*is_output=*/1);

  filter_graph_internal::FilterGraphArgs fg_args = {};
  fg_args.codec_ctx = impl->enc_ctx;
  fg_args.filter_graph = params.filtergraph;
  fg_args.sws_flags = params.sws_flags;
  fg_args.in.pix_fmt = AV_PIX_FMT_GBRPF32;
  fg_args.in.time_base = impl->enc_ctx->time_base;
  fg_args.out.pix_fmt = impl->enc_ctx->pix_fmt;
  if (!filter_graph_internal::ConfigureVideoFilters(
        fg_args, &(impl->graph), &(impl->buffersrc_ctx), &(impl->buffersink_ctx))) {
    return nullptr;
  }

  auto mux_ctx = std::make_unique<MuxContext>();
  mux_ctx->params = params;
  mux_ctx->impl = std::move(impl);
  return mux_ctx;
}

auto MuxWriteFrame(const MuxContext &mux_ctx, const MuxFrame &mux_frame) noexcept -> bool
{
  // clang-format off
  // TODO(tohi): Any way to check if frame_nb is bad? Must be positive?
  if (!(mux_frame.width == mux_ctx.impl->enc_ctx->width && 
        mux_frame.height == mux_ctx.impl->enc_ctx->height && 
        mux_frame.r != nullptr &&
        mux_frame.g != nullptr &&
        mux_frame.b != nullptr)) {
    LogMsg(LogLevel::kError, "Bad frame data input\n");
    return false;
  }
  // clang-format on

  // Pretend that a decoder sent us a frame.
  // Create a frame and fill in the pixel data from the given mux frame.
  AVFrame *dec_frame = av_frame_alloc();
  if (dec_frame == nullptr) {
    LogMsg(LogLevel::kError, "Could not allocate decode frame\n");
    return false;
  }
  dec_frame->format = AV_PIX_FMT_GBRPF32;
  dec_frame->width = mux_frame.width;
  dec_frame->height = mux_frame.height;
  dec_frame->pts = static_cast<int64_t>(mux_frame.frame_nb);
  if (const int ret = av_frame_get_buffer(dec_frame, /*align=*/0); ret < 0) {
    log_utils_internal::LogAvError("Could not allocate decode frame buffer", ret);
    return false;
  }

  const size_t byte_count = static_cast<size_t>(mux_frame.width * mux_frame.height) * sizeof(float);
  std::memcpy(/*__dest=*/dec_frame->data[0], /*__src=*/mux_frame.g, byte_count);
  std::memcpy(/*__dest=*/dec_frame->data[1], /*__src=*/mux_frame.b, byte_count);
  std::memcpy(/*__dest=*/dec_frame->data[2], /*__src=*/mux_frame.r, byte_count);

  return FilterEncodeWriteFrame(mux_ctx.impl->ofmt_ctx,
    mux_ctx.impl->enc_ctx,
    mux_ctx.impl->enc_pkt,
    mux_ctx.impl->enc_frame,
    mux_ctx.impl->buffersrc_ctx,
    mux_ctx.impl->buffersink_ctx,
    dec_frame);
}

auto MuxFinish(const MuxContext &mux_ctx) noexcept -> bool
{
  // Flush filter.
  LogMsg(LogLevel::kInfo, "Flushing filter\n");
  if (!FilterEncodeWriteFrame(mux_ctx.impl->ofmt_ctx,
        mux_ctx.impl->enc_ctx,
        mux_ctx.impl->enc_pkt,
        mux_ctx.impl->enc_frame,
        mux_ctx.impl->buffersrc_ctx,
        mux_ctx.impl->buffersink_ctx,
        /*dec_frame=*/nullptr)) {
    LogMsg(LogLevel::kError, "Cannot flush filter\n");
    return false;
  }

  // Flush encoder.
  if (mux_ctx.impl->enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY) {// NOLINT
    LogMsg(LogLevel::kInfo, "Flushing stream encoder\n");
    if (!EncodeWriteFrame(mux_ctx.impl->ofmt_ctx,
          mux_ctx.impl->enc_ctx,
          mux_ctx.impl->enc_pkt,
          /*enc_frame=*/nullptr)) {
      LogMsg(LogLevel::kError, "Cannot flush stream encoder\n");
      return false;
    }
  }

  if (const int ret = av_write_trailer(mux_ctx.impl->ofmt_ctx); ret < 0) {
    LogMsg(LogLevel::kError, "Cannot write trailer\n");
    return false;
  }

  // Close the output file, if any.
  if (!(mux_ctx.impl->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {// NOLINT
    if (const int ret = avio_closep(&(mux_ctx.impl->ofmt_ctx->pb)); ret < 0) {
      log_utils_internal::LogAvError("Cannot close file", ret);
      return false;
    }
  }
  LogMsg(LogLevel::kInfo, "Closed file\n");

  return true;
}

}// namespace ilp_movie
