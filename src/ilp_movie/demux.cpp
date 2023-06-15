#include <ilp_movie/demux.hpp>

#include <cassert>// assert

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

[[nodiscard]] static auto OpenInputFile(const char *const filename,
  AVFormatContext **ifmt_ctx,
  AVCodecContext **dec_ctx,
  AVPacket **dec_pkt,
  AVFrame **dec_frame,
  int *video_stream_index) -> bool
{
  *dec_pkt = av_packet_alloc();
  if (*dec_pkt == nullptr) {
    ilp_movie::LogError("Cannot not allocate packet");
    return false;
  }

  *dec_frame = av_frame_alloc();
  if (*dec_frame == nullptr) {
    ilp_movie::LogError("Cannot allocate frame");
    return false;
  }

  if (const int ret = avformat_open_input(ifmt_ctx, filename, /*fmt=*/nullptr, /*options=*/nullptr);
      ret < 0) {
    ilp_movie::LogError("Cannot open input file", ret);
    return false;
  }

  if (const int ret = avformat_find_stream_info(*ifmt_ctx, /*options=*/nullptr); ret < 0) {
    ilp_movie::LogError("Cannot find stream information", ret);
    return false;
  }

  // Select the video stream.
  const AVCodec *dec;
  *video_stream_index = av_find_best_stream(*ifmt_ctx,
    AVMEDIA_TYPE_VIDEO,
    /*wanted_stream_nb=*/-1,
    /*related_stream=*/-1,
    &dec,
    /*flags=*/0);
  if (*video_stream_index < 0) {
    ilp_movie::LogError("Cannot find a video stream in the input file", *video_stream_index);
    return false;
  }

  /* create decoding context */
  *dec_ctx = avcodec_alloc_context3(dec);
  if (*dec_ctx == nullptr) { return AVERROR(ENOMEM); }
  avcodec_parameters_to_context(*dec_ctx, (*ifmt_ctx)->streams[*video_stream_index]->codecpar);

  /* init the video decoder */
  if (const int ret = avcodec_open2(*dec_ctx, dec, /*options=*/nullptr); ret < 0) {
    ilp_movie::LogError("Cannot open video decoder", ret);
    return ret;
  }

  return true;
}

namespace ilp_movie {

struct DemuxImpl
{
  AVFormatContext *ifmt_ctx = nullptr;
  AVCodecContext *dec_ctx = nullptr;
  AVPacket *dec_pkt = nullptr;
  AVFrame *dec_frame = nullptr;
  int video_stream_index = -1;
};

auto DemuxInit(DemuxContext *const demux_ctx) noexcept -> bool
{
  // Helper function to make sure we always tear down allocated resources
  // before exiting as a result of failure.
  const auto exit_func = [](const bool success, DemuxContext *const demux_ctx_arg) noexcept {
    if (!success && demux_ctx_arg != nullptr && demux_ctx_arg->impl != nullptr) {
      DemuxFree(demux_ctx_arg);
    }
    return success;
  };

  try {

    if (demux_ctx == nullptr) {
      LogError("Bad demux context");
      return exit_func(/*success=*/false, demux_ctx);
    }
    if (demux_ctx->impl != nullptr) {
      LogError("Found stale demux implementation");
      return exit_func(/*success=*/false, demux_ctx);
    }
    if (demux_ctx->filename == nullptr) {
      LogError("Bad filename");
      return exit_func(/*success=*/false, demux_ctx);
    }

    // Allocate the implementation specific data.
    // MUST be manually free'd at some later point.
    demux_ctx->impl = new DemuxImpl;// NOLINT
    DemuxImpl *impl = demux_ctx->impl;

    if (!OpenInputFile(demux_ctx->filename,
          &impl->ifmt_ctx,
          &impl->dec_ctx,
          &impl->dec_pkt,
          &impl->dec_frame,
          &impl->video_stream_index)) {
      return exit_func(/*success=*/false, demux_ctx);
    }
    assert(impl->video_stream_index >= 0);

    av_dump_format(impl->ifmt_ctx, impl->video_stream_index, demux_ctx->filename, /*is_output=*/0);


  } catch (...) {
    LogError("Unknown exception");
    return exit_func(/*success=*/false, demux_ctx);
  }

  return exit_func(/*success=*/true, demux_ctx);
}

[[nodiscard]] static int64_t FrameToPts(AVStream *st, const int frame)
{
  // NOTE(tohi): Assuming that the stream has constant frame rate!
  const auto frame_rate = st->r_frame_rate;

  return (static_cast<int64_t>(frame) * frame_rate.den * st->time_base.den)
         / (static_cast<int64_t>(frame_rate.num) * st->time_base.num);
}

auto DemuxSeek(const DemuxContext &demux_ctx, const int frame_pos, DemuxFrame *const frame) noexcept
  -> bool
{
  const auto *impl = demux_ctx.impl;
  if (impl == nullptr) { return false; }

  const int64_t seek_target =
    FrameToPts(impl->ifmt_ctx->streams[impl->video_stream_index], frame_pos);
  // seek_target = av_rescale_q(
  //   seek_target, AV_TIME_BASE_Q, impl->ifmt_ctx->streams[impl->video_stream_index]->time_base);

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret =
        av_seek_frame(impl->ifmt_ctx, impl->video_stream_index, seek_target, kSeekFlags);
      ret < 0) {
    LogError("Seek error", ret);
    return false;
  }

  // while(...)
  {
    if (const int ret = av_read_frame(impl->ifmt_ctx, impl->dec_pkt); ret < 0) {
      LogError("Cannot read frame", ret);
      return false;
    }

    if (const int ret = avcodec_send_packet(impl->dec_ctx, impl->dec_pkt); ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Error while sending a packet for decoding");
    }

    int ret = 0;
    while (ret >= 0) {
      ret = avcodec_receive_frame(impl->dec_ctx, impl->dec_frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        LogError("Error while receiving a frame from the decoder", ret);
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
  try {
    if (demux_ctx == nullptr) {
      // Nothing to free!
      return;
    }
    DemuxImpl *impl = demux_ctx->impl;
    if (impl == nullptr) {
      // Nothing to free!
      return;
    }

    avcodec_free_context(&impl->dec_ctx);
    avformat_close_input(&impl->ifmt_ctx);
    av_frame_free(&impl->dec_frame);
    av_packet_free(&impl->dec_pkt);

    delete impl;// NOLINT
    demux_ctx->impl = nullptr;
  } catch (...) {
    LogError("Unknown exception");
  }
}


}// namespace ilp_movie
