#include <ilp_movie/demux.hpp>

#include <cassert>// assert
#include <cerrno>// ENOMEM, etc
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
//#include <libswresample/swresample.h>
}
// clang-format on

#include <ilp_movie/log.hpp>
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
    log_utils_internal::LogAvError("Could not copy decoder parameters to the input stream", ret);
    return false;
  }

  // Init the video decoder.
  if (const int ret = avcodec_open2(codec_ctx, dec, /*options=*/nullptr); ret < 0) {
    log_utils_internal::LogAvError("Cannot open video decoder", ret);
    return false;
  }

  *ifmt_ctx = fmt_ctx;
  *dec_ctx = codec_ctx;
  *video_stream_index = stream_index;

  return true;
}

[[nodiscard]] auto ReadFrame(AVFormatContext *ifmt_ctx,
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

          std::ostringstream oss;
          oss << "frame->pts = " << filt_frame->pts << "\n";
          ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());

          //
          // ProcessFrame(frame)
          // ...
          //
          if (filt_frame->format == AV_PIX_FMT_GBRPF32) {
            auto frame_size = static_cast<size_t>(filt_frame->width);
            frame_size *= static_cast<size_t>(filt_frame->height);
            demux_frame->width = filt_frame->width;
            demux_frame->height = filt_frame->height;
            demux_frame->r.resize(frame_size);
            demux_frame->g.resize(frame_size);
            demux_frame->b.resize(frame_size);
            const size_t byte_count = frame_size * sizeof(float);
            std::memcpy(/*__dest=*/demux_frame->g.data(), /*__src=*/dec_frame->data[0], byte_count);
            std::memcpy(/*__dest=*/demux_frame->b.data(), /*__src=*/dec_frame->data[1], byte_count);
            std::memcpy(/*__dest=*/demux_frame->r.data(), /*__src=*/dec_frame->data[2], byte_count);

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

auto DemuxInit(DemuxContext *const demux_ctx, DemuxFrame *first_frame) noexcept -> bool
{
  // Helper function to make sure we always tear down allocated resources
  // before exiting as a result of failure.
  const auto exit_func = [](const bool success, DemuxContext *const demux_ctx_arg) noexcept {
    if (!success && demux_ctx_arg != nullptr && demux_ctx_arg->impl != nullptr) {
      DemuxFree(demux_ctx_arg);
    }
    return success;
  };

  if (demux_ctx == nullptr) {
    LogMsg(LogLevel::kError, "Bad demux context");
    return exit_func(/*success=*/false, demux_ctx);
  }
  if (demux_ctx->impl != nullptr) {
    LogMsg(LogLevel::kError, "Found stale demux implementation");
    return exit_func(/*success=*/false, demux_ctx);
  }
  if (demux_ctx->filename == nullptr) {
    LogMsg(LogLevel::kError, "Bad filename");
    return exit_func(/*success=*/false, demux_ctx);
  }

  // Allocate the implementation specific data.
  // MUST be manually free'd at some later point.
  demux_ctx->impl = new DemuxImpl;// NOLINT
  DemuxImpl *impl = demux_ctx->impl;

  // Allocate packets and frames.
  impl->dec_pkt = av_packet_alloc();
  impl->dec_frame = av_frame_alloc();
  impl->filt_frame = av_frame_alloc();
  if (!(impl->dec_pkt != nullptr && impl->dec_frame != nullptr && impl->filt_frame != nullptr)) {
    LogMsg(LogLevel::kError, "Cannot allocate frames/packets\n");
    return exit_func(/*success=*/false, demux_ctx);
  }

  if (!OpenInputFile(
        demux_ctx->filename, &(impl->ifmt_ctx), &(impl->dec_ctx), &(impl->video_stream_index))) {
    return exit_func(/*success=*/false, demux_ctx);
  }
  assert(impl->video_stream_index >= 0);// NOLINT

  av_dump_format(impl->ifmt_ctx, impl->video_stream_index, demux_ctx->filename, /*is_output=*/0);

  // "scale=w=0:h=0:out_color_matrix=bt709"
  filter_graph_internal::FilterGraphArgs fg_args = {};
  fg_args.codec_ctx = impl->dec_ctx;
  fg_args.filter_graph = "null";
  fg_args.sws_flags = "";
  fg_args.in.pix_fmt = impl->dec_ctx->pix_fmt;
  fg_args.in.time_base = impl->ifmt_ctx->streams[impl->video_stream_index]->time_base;// NOLINT
  fg_args.out.pix_fmt = AV_PIX_FMT_GBRPF32;
  if (!filter_graph_internal::ConfigureVideoFilters(
        fg_args, &(impl->filter_graph), &(impl->buffersrc_ctx), &(impl->buffersink_ctx))) {
    return exit_func(/*success=*/false, demux_ctx);
  }

  (void)first_frame;
#if 0
  if (!ReadFrame(impl->ifmt_ctx,
        impl->dec_ctx,
        impl->buffersrc_ctx,
        impl->buffersink_ctx,
        impl->dec_pkt,
        impl->dec_frame,
        impl->filt_frame,
        impl->video_stream_index,
        first_frame)) {
    return exit_func(/*success=*/false, demux_ctx);
  }
#endif

  return exit_func(/*success=*/true, demux_ctx);
}

[[nodiscard]] static int64_t FrameToPts(AVStream *st, const int frame)
{
  // NOTE(tohi): Assuming that the stream has constant frame rate!
  const auto frame_rate = st->r_frame_rate;

  return (static_cast<int64_t>(frame) * frame_rate.den * st->time_base.den)
         / (static_cast<int64_t>(frame_rate.num) * st->time_base.num);
}

auto DemuxSeek(const DemuxContext &demux_ctx,
  const int frame_pos,
  DemuxFrame *const /*frame*/) noexcept -> bool
{
  const auto *impl = demux_ctx.impl;
  if (impl == nullptr) { return false; }

  const int64_t seek_target =
    FrameToPts(impl->ifmt_ctx->streams[impl->video_stream_index], frame_pos);// NOLINT
  // seek_target = av_rescale_q(
  //   seek_target, AV_TIME_BASE_Q, impl->ifmt_ctx->streams[impl->video_stream_index]->time_base);

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret =
        av_seek_frame(impl->ifmt_ctx, impl->video_stream_index, seek_target, kSeekFlags);
      ret < 0) {
    log_utils_internal::LogAvError("Seek error", ret);
    return false;
  }

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
}


void DemuxFree(DemuxContext *demux_ctx) noexcept
{
  if (demux_ctx == nullptr) { return; }// Nothing to free!
  DemuxImpl *impl = demux_ctx->impl;
  if (impl == nullptr) { return; }// Nothing to free!

  avfilter_graph_free(&(impl->filter_graph));
  avcodec_free_context(&(impl->dec_ctx));
  avformat_close_input(&(impl->ifmt_ctx));
  av_frame_free(&(impl->dec_frame));
  av_frame_free(&(impl->filt_frame));
  av_packet_free(&(impl->dec_pkt));

  delete impl;// NOLINT
  demux_ctx->impl = nullptr;
}


}// namespace ilp_movie
