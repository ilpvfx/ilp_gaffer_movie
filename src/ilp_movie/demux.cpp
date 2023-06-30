#include <ilp_movie/demux.hpp>

#include <cassert>// assert
#include <cerrno>// ENOMEM, etc
#include <cmath>
#include <cstring>// std::memcpy
#include <sstream>// TMP!!
#include <vector>// std::vector

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
}
// clang-format on

#include <ilp_movie/log.hpp>
#include <ilp_movie/pixel_data.hpp>
#include <internal/dict_utils.hpp>
#include <internal/filter_graph.hpp>
#include <internal/log_utils.hpp>

[[nodiscard]] static auto OpenInputFile(const char *const filename,
  AVFormatContext **ifmt_ctx,
  AVCodecContext **dec_ctx,
  int *video_stream_index) noexcept -> bool
{
  AVFormatContext *fmt_ctx = nullptr;
  if (const int ret = avformat_open_input(&fmt_ctx, filename, /*fmt=*/nullptr, /*options=*/nullptr);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot open input file", ret);
    return false;
  }

  if (const int ret = avformat_find_stream_info(fmt_ctx, /*options=*/nullptr); ret < 0) {
    log_utils_internal::LogAvError("Cannot find stream information", ret);
    return false;
  }

#if 0
  {
    AVDictionary *metadata = nullptr;
    av_dict_copy(&metadata, fmt_ctx->metadata, /*flags=*/0);
    std::ostringstream oss;
    oss << "Metadata: ";
    dict_utils_internal::Options opt = {};
    opt.dict = metadata;
    const auto entries = dict_utils_internal::Entries(opt);
    for (auto &&entry : entries) { oss << "(" << entry.first << " = " << entry.second << "), "; }
    oss << "\n";
    ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
  }
#endif

  // Select the video stream.
  const AVCodec *dec = nullptr;
  const int stream_index = av_find_best_stream(fmt_ctx,
    AVMEDIA_TYPE_VIDEO,
    /*wanted_stream_nb=*/-1,
    /*related_stream=*/-1,
    &dec,
    /*flags=*/0);
  if (stream_index < 0) {
    log_utils_internal::LogAvError("Cannot find a video stream in the input file", stream_index);
    return false;
  }

  // Create decoding context.
  AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
  if (codec_ctx == nullptr) {
    log_utils_internal::LogAvError("Cannot allocate codec context", AVERROR(ENOMEM));
    return false;
  }

  // Copy decoder parameters to the input stream.
  if (const int ret =
        avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[stream_index]->codecpar);// NOLINT
      ret < 0) {
    log_utils_internal::LogAvError("Cannot copy decoder parameters to the input stream", ret);
    return false;
  }

  // Init the video decoder.
  if (const int ret = avcodec_open2(codec_ctx, dec, /*options=*/nullptr); ret < 0) {
    log_utils_internal::LogAvError("Cannot open video decoder", ret);
    return false;
  }

  // Set outputs.
  assert(ifmt_ctx != nullptr && dec_ctx != nullptr && video_stream_index != nullptr);// NOLINT
  *ifmt_ctx = fmt_ctx;
  *dec_ctx = codec_ctx;
  *video_stream_index = stream_index;

  return true;
}

#if 0
[[nodiscard]] static auto ReadFrame(AVFormatContext *ifmt_ctx,
  AVCodecContext *dec_ctx,
  AVFilterContext *buffersrc_ctx,
  AVFilterContext *buffersink_ctx,
  AVPacket *pkt,
  AVFrame *dec_frame,
  AVFrame *filt_frame,
  const int video_stream_index,
  ilp_movie::DemuxFrame *demux_frame) noexcept -> bool
{
  bool got_frame = false;
  int ret = 0;
  while (!got_frame /* && ret >= 0*/) {
    // Read a packet from the file.
    if (ret = av_read_frame(ifmt_ctx, pkt); ret < 0) {
      ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot read packet\n");
      break;
    }

    log_utils_internal::LogPacket(ifmt_ctx, pkt, ilp_movie::LogLevel::kInfo);

    if (pkt->stream_index == video_stream_index) {
      // Send the packet to the decoder.
      if (ret = avcodec_send_packet(dec_ctx, pkt); ret < 0) {
        log_utils_internal::LogAvError("Cannot send packet to decoder", ret);
        break;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, dec_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
          // AVERROR(EAGAIN): Output is not available in this state.
          // AVERROR_EOF: Decoder fully flushed, there will be no more output frames.
          break;
        } else if (ret < 0) {
          log_utils_internal::LogAvError("Cannot receive frame from the decoder", ret);
          goto end;
        }

        dec_frame->pts = dec_frame->best_effort_timestamp;

        // Push the decoded frame into the filter graph.
        if (ret =
              av_buffersrc_add_frame_flags(buffersrc_ctx, dec_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            ret < 0) {
          log_utils_internal::LogAvError("Cannot push decoded frame to the filter graph", ret);
          goto end;
        }

        // Pull filtered frames from the filter graph.
        // while (!got_frame) {
        while (ret >= 0) {
          ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
            break;
          }
          if (ret < 0) { goto end; }

          //
          // ProcessFrame(frame)
          // ...
          //
          if (filt_frame->format == AV_PIX_FMT_GBRPF32) {
            demux_frame->width = filt_frame->width;
            demux_frame->height = filt_frame->height;
            demux_frame->buf.count =
              3 * static_cast<size_t>(demux_frame->width) * static_cast<size_t>(demux_frame->width);
            demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
            const auto r = ilp_movie::ChannelData(demux_frame, ilp_movie::Channel::kRed);
            const auto g = ilp_movie::ChannelData(demux_frame, ilp_movie::Channel::kGreen);
            const auto b = ilp_movie::ChannelData(demux_frame, ilp_movie::Channel::kBlue);
            std::memcpy(// NOLINT
              /*__dest=*/g.data,
              /*__src=*/filt_frame->data[0],
              g.count * sizeof(float));
            std::memcpy(// NOLINT
              /*__dest=*/b.data,
              /*__src=*/filt_frame->data[1],
              b.count * sizeof(float));
            std::memcpy(// NOLINT
              /*__dest=*/r.data,
              /*__src=*/filt_frame->data[2],
              r.count * sizeof(float));

            // got_frame = true;
            goto end;
          }
          // else: unrecognized pixel format!

          // display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base);
          av_frame_unref(filt_frame);
        }
        av_frame_unref(dec_frame);
      }

      // Wipe the packet.
      av_packet_unref(pkt);
    }
    // else: packet is from a stream that we are not interested in (maybe audio?).
  }

end:
  if (ret < 0 && ret != AVERROR_EOF) {// NOLINT
    log_utils_internal::LogAvError("Cannot read frame", ret);
    return false;
  }

  return true;
}
#endif

[[nodiscard]] static constexpr auto AvPixelFormat(const ilp_movie::PixelFormat pix_fmt) noexcept
  -> AVPixelFormat
{
  AVPixelFormat p = AV_PIX_FMT_NONE;
  // clang-format off
  switch (pix_fmt) {
  case ilp_movie::PixelFormat::kGray: p = AV_PIX_FMT_GRAYF32;  break;
  case ilp_movie::PixelFormat::kRGB:  p = AV_PIX_FMT_GBRPF32;  break;
  case ilp_movie::PixelFormat::kRGBA: p = AV_PIX_FMT_GBRAPF32; break;
  default: break;
  }
  // clang-format on
  return p;
}

namespace {

struct SeekEntry
{
  int display_index;
  int64_t first_packet_dts;
  int64_t last_packet_dts;
};

struct SeekTable
{
  std::vector<SeekEntry> entries;
  bool completed = false;
  int num_frames = -1;// total number of frames
  int num_entries = 0;// number of seek-points (keyframes)
  // int allocated_size;
};

}// namespace

namespace ilp_movie {

struct DemuxImpl
{
  AVFormatContext *ifmt_ctx = nullptr;
  AVCodecContext *dec_ctx = nullptr;
  AVPacket *dec_pkt = nullptr;
  AVFrame *dec_frame = nullptr;
  AVFrame *filt_frame = nullptr;
  int video_stream_index = -1;

  AVFilterContext *buffersink_ctx = nullptr;
  AVFilterContext *buffersrc_ctx = nullptr;
  AVFilterGraph *filter_graph = nullptr;

  SeekTable seek_table = {};
};

auto MakeDemuxContext(const DemuxParams &params/*,
  DemuxFrame *first_frame*/) noexcept -> std::unique_ptr<DemuxContext>
{
  if (params.filename == nullptr) {
    LogMsg(LogLevel::kError, "Empty filename\n");
    return nullptr;
  }
  if (AvPixelFormat(params.pix_fmt) == AV_PIX_FMT_NONE) {
    LogMsg(LogLevel::kError, "Unspecified pixel format\n");
    return nullptr;
  }

  // Allocate the implementation specific data. The deleter will be invoked if we leave this
  // function before moving the implementation to the context.
  auto impl = demux_impl_ptr(new DemuxImpl(), [](DemuxImpl *impl_ptr) noexcept {
    LogMsg(LogLevel::kInfo, "Free demux implementation\n");
    assert(impl_ptr != nullptr);// NOLINT

    avfilter_graph_free(&(impl_ptr->filter_graph));
    avcodec_free_context(&(impl_ptr->dec_ctx));
    avformat_close_input(&(impl_ptr->ifmt_ctx));
    av_frame_free(&(impl_ptr->dec_frame));
    av_frame_free(&(impl_ptr->filt_frame));
    av_packet_free(&(impl_ptr->dec_pkt));
    delete impl_ptr;// NOLINT
  });

  // Allocate packets and frames.
  impl->dec_pkt = av_packet_alloc();
  impl->dec_frame = av_frame_alloc();
  impl->filt_frame = av_frame_alloc();
  if (!(impl->dec_pkt != nullptr && impl->dec_frame != nullptr && impl->filt_frame != nullptr)) {
    LogMsg(LogLevel::kError, "Cannot allocate frames/packets\n");
    return nullptr;
  }

  if (!OpenInputFile(
        params.filename, &(impl->ifmt_ctx), &(impl->dec_ctx), &(impl->video_stream_index))) {
    return nullptr;
  }
  assert(impl->video_stream_index >= 0);// NOLINT

  av_dump_format(impl->ifmt_ctx, impl->video_stream_index, params.filename, /*is_output=*/0);

  // "scale=w=0:h=0:out_color_matrix=bt709"
  filter_graph_internal::FilterGraphArgs fg_args = {};
  fg_args.codec_ctx = impl->dec_ctx;
  fg_args.filter_graph = "null";
  fg_args.sws_flags = "";
  fg_args.in.pix_fmt = impl->dec_ctx->pix_fmt;
  fg_args.in.time_base = impl->ifmt_ctx->streams[impl->video_stream_index]->time_base;// NOLINT
  fg_args.out.pix_fmt = AvPixelFormat(params.pix_fmt);
  if (!filter_graph_internal::ConfigureVideoFilters(
        fg_args, &(impl->filter_graph), &(impl->buffersrc_ctx), &(impl->buffersink_ctx))) {
    return nullptr;
  }

#if 0
  DemuxFrame first_frame = {};
  if (!ReadFrame(impl->ifmt_ctx,
        impl->dec_ctx,
        impl->buffersrc_ctx,
        impl->buffersink_ctx,
        impl->dec_pkt,
        impl->dec_frame,
        impl->filt_frame,
        impl->video_stream_index,
        &first_frame)) {
    return nullptr;
  }
  (void)first_frame;
#else
  //(void)first_frame;
#endif

  auto demux_ctx = std::make_unique<DemuxContext>();
  demux_ctx->params = params;
  demux_ctx->impl = std::move(impl);
  return demux_ctx;
}


#if 0
[[nodiscard]] static auto FrameToPts(const AVStream &st, const int frame_index) noexcept -> int64_t
{
  // NOTE(tohi): Assuming that the stream has constant frame rate!
  const auto frame_rate = st.r_frame_rate;

  return static_cast<int64_t>(
    std::round(static_cast<double>(frame_index * frame_rate.den * st.time_base.den)
               / static_cast<double>(frame_rate.num) * st.time_base.num));
}
#endif

[[nodiscard]] static auto FrameIndexToTimestamp(const int frame_index,
  const AVRational &frame_rate,
  const AVRational &time_base,
  const int64_t start_time) noexcept -> int64_t
{
  // Rationale:
  //
  // f = t * (fr.num / fr.den)   <=>   t = f * (fr.den / fr.num)
  //
  // where f is the (integer) frame index, t is time in seconds, and fr is the frame rate (expressed
  // as frames / second, e.g. [24 / 1]). Now we want to convert the time, t, into a timestamp
  // (integer), ts, in the provided time base (tb):
  //
  // t = (ts + ts_0) * (tb.num / tb.den)   <=>   ts = t * (tb.den / tb.num) - ts_0
  //
  // Substituting in our expression for t from above:
  //
  // ts = f * (fr.den * tb.den / fr.num * tb.num) - ts_0
  //
  // NOTE(tohi) that we assume a constant frame rate here!

  return static_cast<int64_t>(
           std::round(frame_index * av_q2d(av_mul_q(av_inv_q(frame_rate), av_inv_q(time_base)))))
         - (start_time != AV_NOPTS_VALUE ? start_time : 0);
}


auto DemuxSeek(const DemuxContext &demux_ctx,
  const int frame_index,
  DemuxFrame *const demux_frame) noexcept -> bool
{
  const auto *impl = demux_ctx.impl.get();
  assert(impl != nullptr);// NOLINT

  assert(0 <= impl->video_stream_index// NOLINT
         && impl->video_stream_index < static_cast<int>(impl->ifmt_ctx->nb_streams));
  const AVStream *video_stream = impl->ifmt_ctx->streams[impl->video_stream_index];// NOLINT
  assert(video_stream != nullptr);// NOLINT

  // Assume constant frame rate.
  const AVRational frame_rate = video_stream->r_frame_rate;
  (void)frame_rate;

  {
    std::ostringstream oss;
    oss << "Stream | nb_frames: " << video_stream->nb_frames
        << ", frame_rate: " << video_stream->r_frame_rate.num << "/"
        << video_stream->r_frame_rate.den << ", start_time: " << video_stream->start_time
        << ", time_base: " << video_stream->time_base.num << "/" << video_stream->time_base.den
        << "\n";
    LogMsg(LogLevel::kInfo, oss.str().c_str());
  }
  {
    std::ostringstream oss;
    oss << "Codec | framerate. " << impl->dec_ctx->framerate.num << "/"
        << impl->dec_ctx->framerate.den << "\n";
    LogMsg(LogLevel::kInfo, oss.str().c_str());
  }

  // Remove first dts: when non zero seek should be more accurate
  // const auto first_pts_usecs = static_cast<int64_t>(
  //   std::round(static_cast<double>(video_stream->start_time * video_stream->time_base.num)
  //              / static_cast<double>(video_stream->time_base.den * AV_TIME_BASE)));
  // target_dts_usecs += first_dts_usecs;

  // Seek is done on packet dts.
  int64_t target_pts = FrameIndexToTimestamp(
    frame_index, video_stream->r_frame_rate, video_stream->time_base, video_stream->start_time);

  {
    std::ostringstream oss;
    oss << "target_pts: " << target_pts << "\n";
    LogMsg(LogLevel::kInfo, oss.str().c_str());
  }

  // if (impl->dec_ctx->framerate.num == 0 && impl->dec_ctx->framerate.den == 1) {
  //   LogMsg(LogLevel::kError, "Unknown codec framerate\n");
  //   return false;
  // }

  // const int64_t seek_target =
  //   FrameToPts(impl->ifmt_ctx->streams[impl->video_stream_index], frame_pos);// NOLINT
  // seek_target = av_rescale_q(
  //   seek_target, AV_TIME_BASE_Q, impl->ifmt_ctx->streams[impl->video_stream_index]->time_base);

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret =
        av_seek_frame(impl->ifmt_ctx, impl->video_stream_index, target_pts, kSeekFlags);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot seek to timestamp", ret);
    return false;
  }
  avcodec_flush_buffers(impl->dec_ctx);

  AVPacket *pkt = impl->dec_pkt;

  bool got_frame = false;
  int ret = 0;
  do {
    ret = av_read_frame(impl->ifmt_ctx, pkt);

    // Ignore packet that are not from the video stream we are interested in.
    if (ret >= 0 && pkt->stream_index != impl->video_stream_index) {
      av_packet_unref(pkt);
      continue;
    }
    log_utils_internal::LogPacket(impl->ifmt_ctx, impl->dec_pkt, ilp_movie::LogLevel::kInfo);

    if (ret < 0) {
      // Send flush packet to decoder.
      ret = avcodec_send_packet(impl->dec_ctx, nullptr);
    } else {
      if (pkt->pts == AV_NOPTS_VALUE) {
        LogMsg(LogLevel::kError, "Missing packet/frame PTS value\n");
        return false;
      }
      ret = avcodec_send_packet(impl->dec_ctx, pkt);
    }

    av_packet_unref(pkt);

    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot submit a packet for decoding", ret);
      return false;
    }

    while (ret >= 0) {
      ret = avcodec_receive_frame(impl->dec_ctx, impl->dec_frame);
      if (ret == AVERROR_EOF) {// NOLINT
        goto end;// should this be an error if we haven't found our seek frame yet!?
      } else if (ret == AVERROR(EAGAIN)) {
        ret = 0;
        break;
      } else if (ret < 0) {
        log_utils_internal::LogAvError("Cannot decode frame", ret);
        return false;
      }

      {
        std::ostringstream oss;
        oss << "Frame | pts: " << impl->dec_frame->pts
            << " | best_effort_ts: " << impl->dec_frame->best_effort_timestamp
            << " | pkt_duration: " << impl->dec_frame->pkt_duration
            << " | key_frame: " << impl->dec_frame->key_frame
            << " | sample_aspect_ratio: " << impl->dec_frame->sample_aspect_ratio.num << "/"
            << impl->dec_frame->sample_aspect_ratio.den << "\n";
        LogMsg(LogLevel::kInfo, oss.str().c_str());
      }

      impl->dec_frame->pts = impl->dec_frame->best_effort_timestamp;
      if (impl->dec_frame->pts == AV_NOPTS_VALUE) {
        // error! b-frame?
      }

      // Check if the decoded frame has a PTS/duration that matches our seek target. If so, we will
      // push the frame through the filter graph.
      if (impl->dec_frame->pts <= target_pts
          && target_pts < (impl->dec_frame->pts + impl->dec_frame->pkt_duration)) {
        // Got a frame whose duration includes our seek target.

        // Send the decoded frame to the filter graph.
        if (ret = av_buffersrc_add_frame_flags(
              impl->buffersrc_ctx, impl->dec_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            ret < 0) {
          log_utils_internal::LogAvError("Cannot push decoded frame to the filter graph", ret);
          goto end;
        }

        // Get filtered frames from the filter graph.
        while (ret >= 0) {
          ret = av_buffersink_get_frame(impl->buffersink_ctx, impl->filt_frame);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
            break;
          }
          if (ret < 0) { goto end; }

          // Transfer frame data to output frame.
          demux_frame->width = impl->filt_frame->width;
          demux_frame->height = impl->filt_frame->height;
          demux_frame->frame_index = frame_index;
          demux_frame->key_frame = impl->filt_frame->key_frame > 0;
          demux_frame->pixel_aspect_ratio = 1.0;
          if (impl->dec_frame->sample_aspect_ratio.num != 0
              && impl->dec_frame->sample_aspect_ratio.den != 1) {
            demux_frame->pixel_aspect_ratio = av_q2d(impl->dec_frame->sample_aspect_ratio);
          }

          //
          // ProcessFrame(frame)
          // ...
          //
          PixelData<float> r = {};
          PixelData<float> g = {};
          PixelData<float> b = {};
          PixelData<float> a = {};
          // clang-format off
          switch (impl->filt_frame->format) {
          case AV_PIX_FMT_GRAYF32:
            demux_frame->pix_fmt = PixelFormat::kGray;
            demux_frame->buf.count = ChannelCount(demux_frame->pix_fmt)
                                     * static_cast<size_t>(demux_frame->width)
                                     * static_cast<size_t>(demux_frame->width);
            demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
            r = ChannelData(demux_frame, Channel::kGray);
            std::memcpy(r.data, impl->filt_frame->data[0], r.count * sizeof(float));// NOLINT
            break;
          case AV_PIX_FMT_GBRPF32:
            demux_frame->pix_fmt = PixelFormat::kRGB;
            demux_frame->buf.count = ChannelCount(demux_frame->pix_fmt)
                                     * static_cast<size_t>(demux_frame->width)
                                     * static_cast<size_t>(demux_frame->width);
            demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
            r = ChannelData(demux_frame, Channel::kRed);
            g = ChannelData(demux_frame, Channel::kGreen);
            b = ChannelData(demux_frame, Channel::kBlue);
            std::memcpy(g.data, impl->filt_frame->data[0], g.count * sizeof(float));// NOLINT
            std::memcpy(b.data, impl->filt_frame->data[1], b.count * sizeof(float));// NOLINT
            std::memcpy(r.data, impl->filt_frame->data[2], r.count * sizeof(float));// NOLINT
            break;
          case AV_PIX_FMT_GBRAPF32:
            demux_frame->pix_fmt = PixelFormat::kRGBA;
            demux_frame->buf.count = ChannelCount(demux_frame->pix_fmt)
                                     * static_cast<size_t>(demux_frame->width)
                                     * static_cast<size_t>(demux_frame->width);
            demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
            r = ChannelData(demux_frame, Channel::kRed);
            g = ChannelData(demux_frame, Channel::kGreen);
            b = ChannelData(demux_frame, Channel::kBlue);
            a = ChannelData(demux_frame, Channel::kAlpha);
            std::memcpy(g.data, impl->filt_frame->data[0], g.count * sizeof(float));// NOLINT
            std::memcpy(b.data, impl->filt_frame->data[1], b.count * sizeof(float));// NOLINT
            std::memcpy(r.data, impl->filt_frame->data[2], r.count * sizeof(float));// NOLINT
            std::memcpy(a.data, impl->filt_frame->data[3], a.count * sizeof(float));// NOLINT
            break;
          default:
            LogMsg(LogLevel::kError, "Unsupported pixel format\n");
            return false;
          }
          // clang-format off
          // else: unrecognized pixel format!

          // display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base);

          av_frame_unref(impl->filt_frame);
          got_frame = true;
        }
      }

      av_frame_unref(impl->dec_frame);
    }

  } while (ret >= 0);

end:
  return got_frame;

#if 0
    // Read frames until we find the one we are seeking.
    // For video: one packet == one frame.
    int ret = 0;
  while (av_read_frame(impl->ifmt_ctx, impl->dec_pkt) >= 0) {
    log_utils_internal::LogPacket(impl->ifmt_ctx, impl->dec_pkt, ilp_movie::LogLevel::kInfo);


    // Read a packet from the file.
    // if (ret = av_read_frame(impl->ifmt_ctx, impl->dec_pkt); ret < 0) {
    //   ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Cannot read packet\n");
    //   break;
    // }

    if (impl->dec_pkt->stream_index == impl->video_stream_index) {
      // Send the packet to the decoder.
      if (ret = avcodec_send_packet(impl->dec_ctx, impl->dec_pkt); ret < 0) {
        log_utils_internal::LogAvError("Cannot send packet to decoder", ret);
        break;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(impl->dec_ctx, impl->dec_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
          // AVERROR(EAGAIN): Output is not available in this state.
          // AVERROR_EOF: Decoder fully flushed, there will be no more output frames.
          break;
        } else if (ret == AVERROR(EINVAL)) {
          // AVERROR(EINVAL): Codec not opened or it is an encoder.
          return false;
        } else if (ret < 0) {
          log_utils_internal::LogAvError("Cannot receive frame from the decoder", ret);

          //          goto end;
        }

        impl->dec_frame->pts = impl->dec_frame->best_effort_timestamp;

        {
          std::ostringstream oss;
          oss << "Frame pts: " << impl->dec_frame->pts << "\n";
          LogMsg(LogLevel::kInfo, oss.str().c_str());
        }


        // Wipe the frame buffers.
        av_frame_unref(impl->dec_frame);
      }
    }
    // else: The packet is not from the video stream we are interested in.

    // Wipe the packet buffer.
    av_packet_unref(impl->dec_pkt);

    if (ret < 0) { break; }
  }
#endif

  // return ReadFrame(impl->ifmt_ctx,
  //   impl->dec_ctx,
  //   impl->buffersrc_ctx,
  //   impl->buffersink_ctx,
  //   impl->dec_pkt,
  //   impl->dec_frame,
  //   impl->filt_frame,
  //   impl->video_stream_index,
  //   demux_frame);
  (void)demux_frame;

  return true;

#if 0

  // while(...)
  {
    if (const int ret = av_read_frame(impl->ifmt_ctx, impl->dec_pkt); ret < 0) {
      log_utils_internal::LogAvError("Cannot read frame", ret);
      return false;
    }

    if (const int ret = avcodec_send_packet(impl->dec_ctx, impl->dec_pkt); ret < 0) {
      log_utils_internal::LogAvError("Error while sending a packet for decoding", ret);
      return false;
    }

    int ret = 0;
    while (ret >= 0) {
      ret = avcodec_receive_frame(impl->dec_ctx, impl->dec_frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {// NOLINT
        break;
      } else if (ret < 0) {
        log_utils_internal::LogAvError("Error while receiving a frame from the decoder", ret);
        return false;
      }

      impl->dec_frame->pts = impl->dec_frame->best_effort_timestamp;

      /*if (impl->dec_frame->best_effort_timestamp)*/
      {
        // convert frame to RGB format
#if 0
        dec_frame->format = AV_PIX_FMT_GBRPF32;

        frame->width = impl->dec_frame->width;
        frame->height = impl->dec_frame->height;
        frame->r = impl->dec_frame->data
        frame->g = 
        frame->b =
#endif
      }
      av_frame_unref(impl->dec_frame);
    }
  }


  return true;
#endif
}

}// namespace ilp_movie


#if 0
  const PixelFormat pix_fmt = std::invoke([&] {
    switch (frame_pos % 3) {
    case 0:
      return PixelFormat::kGray;
    case 1:
      return PixelFormat::kRGB;
    case 2:
      return PixelFormat::kRGBA;
    default:
      return PixelFormat::kNone;
    }
  });

  // TMP!!
  const auto cx = 0.5F * static_cast<float>(impl->dec_ctx->width) + static_cast<float>(frame_pos);
  const auto cy = 0.5F * static_cast<float>(impl->dec_ctx->height);
  constexpr auto kR = 100.F;// in pixels

  frame->width = impl->dec_ctx->width;
  frame->height = impl->dec_ctx->height;
  frame->pixel_aspect_ratio = 1.0;
  frame->pix_fmt = pix_fmt;
  frame->buf.count =
    ChannelCount(pix_fmt) * static_cast<size_t>(frame->width) * static_cast<size_t>(frame->height);
  frame->buf.data = std::make_unique<float[]>(frame->buf.count);// NOLINT

  const auto r = ChannelData(
    frame->width, frame->height, Channel::kRed, frame->buf.data.get(), frame->buf.count);
  const auto g = ChannelData(
    frame->width, frame->height, Channel::kGreen, frame->buf.data.get(), frame->buf.count);
  const auto b = ChannelData(
    frame->width, frame->height, Channel::kBlue, frame->buf.data.get(), frame->buf.count);
  const auto a = ChannelData(
    frame->width, frame->height, Channel::kAlpha, frame->buf.data.get(), frame->buf.count);
  for (int y = 0; y < frame->height; ++y) {
    for (int x = 0; x < frame->width; ++x) {
      const auto i =
        static_cast<size_t>(y) * static_cast<size_t>(frame->width) + static_cast<size_t>(x);
      const auto xx = static_cast<float>(x);
      const auto yy = static_cast<float>(y);
      const float d = std::sqrt((xx - cx) * (xx - cx) + (yy - cy) * (yy - cy)) - kR;

      switch (frame->pix_fmt) {
      case PixelFormat::kGray:
        r.data[i] = std::abs(d);// NOLINT
        r.data[i] /=// NOLINT
          static_cast<float>(std::sqrt(impl->dec_ctx->width * impl->dec_ctx->width
                                       + impl->dec_ctx->height * impl->dec_ctx->height));
        break;
      case PixelFormat::kRGB:
        if (d < 0) {
          b.data[i] = std::abs(d);// NOLINT
          b.data[i] /=// NOLINT
            static_cast<float>(std::sqrt(impl->dec_ctx->width * impl->dec_ctx->width
                                         + impl->dec_ctx->height * impl->dec_ctx->height));
        } else {
          r.data[i] = d;// NOLINT
          r.data[i] /=// NOLINT
            static_cast<float>(std::sqrt(impl->dec_ctx->width * impl->dec_ctx->width
                                         + impl->dec_ctx->height * impl->dec_ctx->height));
        }
        g.data[i] = 0.F;// NOLINT
        break;
      case PixelFormat::kRGBA:
        if (d < 0) {
          r.data[i] = std::abs(d);// NOLINT
          r.data[i] /=// NOLINT
            static_cast<float>(std::sqrt(impl->dec_ctx->width * impl->dec_ctx->width
                                         + impl->dec_ctx->height * impl->dec_ctx->height));
        } else {
          g.data[i] = d;// NOLINT
          g.data[i] /=// NOLINT
            static_cast<float>(std::sqrt(impl->dec_ctx->width * impl->dec_ctx->width
                                         + impl->dec_ctx->height * impl->dec_ctx->height));
        }
        b.data[i] = 0.F;// NOLINT
        a.data[i] = (d < 0) ? 1.F : 0.F;// NOLINT
        break;
      case PixelFormat::kNone:
        break;
      }
    }
  }
#endif
