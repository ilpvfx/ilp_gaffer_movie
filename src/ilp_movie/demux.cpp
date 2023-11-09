#include <ilp_movie/demux.hpp>

#include <cassert>// assert
#include <cerrno>// ENOMEM, etc
#include <cmath>
#include <cstring>// std::memcpy
#include <sstream>// TMP!!
#include <type_traits>// std::is_same_v
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
#include <internal/timestamp.hpp>

static_assert(std::is_same_v<std::int64_t, int64_t>);

namespace {
constexpr struct
{
  bool enabled = true;
  int log_level = ilp_movie::LogLevel::kInfo;
} kTrace = {};

template<typename ResourceT> class RefHolder
{
public:
  static_assert(std::is_same_v<ResourceT, AVPacket> || std::is_same_v<ResourceT, AVFrame>);

  explicit RefHolder(ResourceT *const ptr) noexcept : _ptr(ptr) {}
  RefHolder(const RefHolder &) noexcept = delete;
  RefHolder(RefHolder &&) noexcept = delete;
  RefHolder &operator=(const RefHolder &) noexcept = delete;
  RefHolder &operator=(RefHolder &&) noexcept = delete;

  ~RefHolder() noexcept { unref(); }

  [[nodiscard]] auto get() const noexcept -> ResourceT * { return _ptr; }

  void unref() noexcept
  {
    if (_ptr != nullptr && !_released) {
      if constexpr (std::is_same_v<ResourceT, AVPacket>) {
        av_packet_unref(_ptr);
        _released = true;
      } else if constexpr (std::is_same_v<ResourceT, AVFrame>) {
        av_frame_unref(_ptr);
        _released = true;
      } else {
        static_assert(sizeof(ResourceT) == 0U); // Fail.
      }
    }
  }

private:
  ResourceT *_ptr = nullptr;
  bool _released = false;
};

using PacketRefHolder = RefHolder<AVPacket>;
using FrameRefHolder = RefHolder<AVFrame>;

}// namespace

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
    log_utils_internal::LogAvError("Cannot copy stream parameters to the decoder", ret);
    return false;
  }

  if constexpr (kTrace.enabled) {
    std::ostringstream oss;
    oss << "color_range: " << av_color_range_name(codec_ctx->color_range) << "\n";
    ilp_movie::LogMsg(kTrace.log_level, oss.str().c_str());
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


[[nodiscard]] static auto StreamFrameRate(AVFormatContext *const fmt_ctx,
  const int video_stream_index) noexcept -> AVRational
{
  // Default to 1 fps.
  AVRational fr = {};
  fr.num = 1;
  fr.den = 1;

  // If frame rate is specified, use it.
  const AVStream *video_stream = fmt_ctx->streams[video_stream_index];// NOLINT
  if (video_stream->r_frame_rate.num != 0 && video_stream->r_frame_rate.den != 0) {
    fr.num = video_stream->r_frame_rate.num;
    fr.den = video_stream->r_frame_rate.den;
  }
  return fr;
}


template<typename FilterFuncT>
[[nodiscard]] static auto SeekFrame(AVFormatContext *const fmt_ctx,
  AVCodecContext *const dec_ctx,
  AVPacket *const dec_pkt,
  AVFrame *const dec_frame,
  const int video_stream_index,
  const int frame_index,
  FilterFuncT &&filter_func) noexcept -> bool
{
  assert(0 <= video_stream_index// NOLINT
         && video_stream_index < static_cast<int>(fmt_ctx->nb_streams));
  const AVStream *video_stream = fmt_ctx->streams[video_stream_index];// NOLINT
  assert(video_stream != nullptr);// NOLINT

  // Seek is done on frame pts.
  // NOTE: Frame index is zero-based.
  const AVRational frame_rate = StreamFrameRate(fmt_ctx, video_stream_index);
  const int64_t start_pts = 0;//StreamStartPts(fmt_ctx, dec_ctx, video_stream_index);
  const int64_t target_pts = timestamp_internal::FrameToPts(frame_index,
    frame_rate.num,
    frame_rate.den,
    video_stream->time_base.num,
    video_stream->time_base.den,
    start_pts);

  if constexpr (kTrace.enabled) {
    {
      std::ostringstream oss;
      oss << "Stream | nb_frames: " << video_stream->nb_frames
          << ", frame_rate: " << video_stream->r_frame_rate.num << "/"
          << video_stream->r_frame_rate.den << ", start_time: " << video_stream->start_time
          << ", time_base: " << video_stream->time_base.num << "/" << video_stream->time_base.den
          << "\n";
      ilp_movie::LogMsg(kTrace.log_level, oss.str().c_str());
    }
    {
      std::ostringstream oss;
      oss << "Codec | framerate. " << dec_ctx->framerate.num << "/" << dec_ctx->framerate.den
          << "\n";
      ilp_movie::LogMsg(kTrace.log_level, oss.str().c_str());
    }
    {
      std::ostringstream oss;
      oss << "target_pts: " << target_pts << "\n";
      ilp_movie::LogMsg(kTrace.log_level, oss.str().c_str());
    }
  }

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret = av_seek_frame(fmt_ctx, video_stream_index, target_pts, kSeekFlags); ret < 0) {
    log_utils_internal::LogAvError("Cannot seek to timestamp", ret);
    return false;
  }
  avcodec_flush_buffers(dec_ctx);

  bool got_frame = false;
  int ret = 0;
  do {
    // Read a packet, which for video streams corresponds to one frame.
    PacketRefHolder pkt{ dec_pkt };
    ret = av_read_frame(fmt_ctx, pkt.get());

    // Ignore packets that are not from the video stream we are trying to read from.
    if (ret >= 0 && pkt.get()->stream_index != video_stream_index) {
      pkt.unref();
      continue;
    }

    if constexpr (kTrace.enabled) {
      // log_utils_internal::LogPacket(fmt_ctx, pkt.get(), ilp_movie::LogLevel::kInfo);
    }

    if (ret < 0) {
      // Failed to read frame - send flush packet to decoder.
      ret = avcodec_send_packet(dec_ctx, /*avpkt=*/nullptr);
    } else {
      if (pkt.get()->pts == AV_NOPTS_VALUE) {
        ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Missing packet PTS (B-frame?)\n");
        return false;
      }
      ret = avcodec_send_packet(dec_ctx, pkt.get());
    }

    // Packet has been sent to the decoder.
    pkt.unref();

    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot submit a packet for decoding", ret);
      return false;
    }

    while (ret >= 0) {
      FrameRefHolder frame{ dec_frame };
      ret = avcodec_receive_frame(dec_ctx, frame.get());
      if (ret == AVERROR_EOF) {// NOLINT
        goto end;// should this be an error if we haven't found our seek frame yet!?
      } else if (ret == AVERROR(EAGAIN)) {
        ret = 0;
        break;
      } else if (ret < 0) {
        log_utils_internal::LogAvError("Cannot decode frame", ret);
        return false;
      }

      if constexpr (kTrace.enabled) {
        std::ostringstream oss;
        oss << "Frame | pts: " << frame.get()->pts
            << " | best_effort_ts: " << frame.get()->best_effort_timestamp
            << " | pkt_duration: " << frame.get()->pkt_duration
            << " | key_frame: " << frame.get()->key_frame
            << " | sample_aspect_ratio: " << frame.get()->sample_aspect_ratio.num << "/"
            << frame.get()->sample_aspect_ratio.den
            << " | codec frame number: " << dec_ctx->frame_number << "\n";
        ilp_movie::LogMsg(kTrace.log_level, oss.str().c_str());
      }

      frame.get()->pts = frame.get()->best_effort_timestamp;
      const int64_t pts = frame.get()->pts;
      if (pts == AV_NOPTS_VALUE) {
        ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Missing frame PTS\n");
        return false;
      }

      // Check if the decoded frame has a PTS/duration that matches our seek target. If so, we will
      // push the frame through the filter graph.
      if (pts <= target_pts && target_pts < (pts + frame.get()->pkt_duration)) {
        // Got a frame whose duration includes our seek target.
        return filter_func(frame.get());

        // av_frame_unref(impl->filt_frame);
        // got_frame = true;
        // goto end;
      }

      // Frame gets unref'd by the holder at the end of scope.
    }

  } while (ret >= 0 && !got_frame);

end:
  return got_frame;
}

[[nodiscard]] static auto QueryVideoInfo(AVFormatContext *const fmt_ctx,
  AVCodecContext *const dec_ctx,
  AVPacket *const dec_pkt,
  AVFrame *const dec_frame,
  const int video_stream_index,
  ilp_movie::DemuxVideoInfo *const vi) noexcept -> bool
{
  assert(0 <= video_stream_index// NOLINT
         && video_stream_index < static_cast<int>(fmt_ctx->nb_streams));
  const AVStream *video_stream = fmt_ctx->streams[video_stream_index];// NOLINT
  assert(video_stream != nullptr);// NOLINT

  const auto filter_func = [&](AVFrame *f) noexcept {
    if constexpr (kTrace.enabled) {
      std::ostringstream oss;
      oss << "Frame | display_picture_number: " << f->display_picture_number
          << " | planes: " << av_pix_fmt_count_planes(static_cast<AVPixelFormat>(f->format))
          << " | pix_fmt: '" << av_get_pix_fmt_name(static_cast<AVPixelFormat>(f->format)) << "'"
          << "\n";

      ilp_movie::LogMsg(kTrace.log_level, oss.str().c_str());
    }
    assert(f != nullptr);// NOLINT

    const AVRational frame_rate = StreamFrameRate(fmt_ctx, video_stream_index);
    //const int64_t start_pts = 0;//StreamStartPts(fmt_ctx, dec_ctx, video_stream_index);
    vi->width = f->width;
    vi->height = f->height;
    vi->frame_rate = av_q2d(frame_rate);
#if 0
    vi->first_frame_nb =
      timestamp_internal::PtsToFrame(0,// TODO(tohi): should be pts of first frame!!!
        frame_rate.num,
        frame_rate.den,
        video_stream->time_base.num,
        video_stream->time_base.den,
        start_pts)
      + 1;
#else
    vi->first_frame_nb = 1;
#endif
    vi->frame_count = 0;//StreamFrameCount();// video_stream->nb_frames;

    // NOTE(tohi): This is a WILD guess. It might be better to just let the user
    //             provide the desired output pixel format.
    // clang-format off
    vi->pix_fmt = ilp_movie::PixelFormat::kNone;
    switch (av_pix_fmt_count_planes(static_cast<AVPixelFormat>(f->format))) {
      case 1: vi->pix_fmt = ilp_movie::PixelFormat::kGray; break;   
      case 3: vi->pix_fmt = ilp_movie::PixelFormat::kRGB;  break;
      case 4: vi->pix_fmt = ilp_movie::PixelFormat::kRGBA; break;
      default: break;
    }
    // clang-format on
    return vi->pix_fmt != ilp_movie::PixelFormat::kNone;
  };

  // Take a look at the first frame to gather some information.
  return SeekFrame(fmt_ctx,
    dec_ctx,
    dec_pkt,
    dec_frame,
    video_stream_index,
    /*frame_index=*/0,
    filter_func);
}

// Caller is responsible for freeing (av_frame_unref) the filtered frame after it has been used.
[[nodiscard]] static auto FilterFrame(AVFilterContext *const buffersrc_ctx,
  AVFilterContext *const buffersink_ctx,
  AVFrame *const dec_frame,
  AVFrame *const filt_frame) noexcept -> bool
{
  // Push the decoded frame into the filter graph.
  if (const int ret =
        av_buffersrc_add_frame_flags(buffersrc_ctx, dec_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot push decoded frame to the filter graph", ret);
    return false;
  }

  int frame_count = 0;
  int ret = 0;
  for (;;) {
    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
    if (ret >= 0) {
      ++frame_count;
      assert(frame_count == 1);// NOLINT
      // Keep going, interesting to see how many frames we get (hopefully only one).
    } else {
      // This is the only way for the loop to end.
      break;
    }
  }

  // AVERROR(EAGAIN): No frames are available at this point; more input frames must be added
  // to the filter graph to get more output.
  //
  // AVERROR_EOF: There will be no more output frames on this sink.
  //
  // These are not really errors, at least not if we already managed to read one frame.
  assert(ret < 0);// NOLINT
  const bool err = ret != AVERROR(EAGAIN) && ret != AVERROR_EOF;// NOLINT
  if (err) {
    // An actual error has occured.
    log_utils_internal::LogAvError("Cannot get frame from the filter graph", ret);
  }

  // We are expecting to get exactly one frame.
  return !err && frame_count == 1;
}

// Returns AV_PIX_FMT_NONE if no suitable conversion could be found.
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

auto DemuxMakeContext(const DemuxParams &params/*,
  DemuxFrame *first_frame*/) noexcept -> std::unique_ptr<DemuxContext>
{
  if (params.filename.empty()) {
    LogMsg(LogLevel::kError, "Empty filename\n");
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

  if (!OpenInputFile(params.filename.c_str(),
        &(impl->ifmt_ctx),
        &(impl->dec_ctx),
        &(impl->video_stream_index))) {
    return nullptr;
  }
  assert(impl->video_stream_index >= 0);// NOLINT

  av_dump_format(
    impl->ifmt_ctx, impl->video_stream_index, params.filename.c_str(), /*is_output=*/0);

  // Determine if video stream is Gray | RGB | RGBA
  DemuxVideoInfo video_info = {};
  if (!QueryVideoInfo(impl->ifmt_ctx,
        impl->dec_ctx,
        impl->dec_pkt,
        impl->dec_frame,
        impl->video_stream_index,
        &video_info)) {
    return nullptr;
  }

  const AVPixelFormat av_pix_fmt = AvPixelFormat(video_info.pix_fmt);
  if (av_pix_fmt == AV_PIX_FMT_NONE) {
    LogMsg(LogLevel::kError, "Cannot determine output pixel format\n");
    return nullptr;
  }

  // filter_graph_internal::FilterGraphArgs fg_args = {};
  // fg_args.codec_ctx = impl->dec_ctx;
  // fg_args.filter_graph = params.filter_graph;
  // fg_args.sws_flags = params.sws_flags;
  // fg_args.in.pix_fmt = impl->dec_ctx->pix_fmt;
  // fg_args.in.time_base = impl->ifmt_ctx->streams[impl->video_stream_index]->time_base;// NOLINT
  // fg_args.out.pix_fmt = AvPixelFormat(video_info.pix_fmt);
  // if (!filter_graph_internal::ConfigureVideoFilters(
  //       fg_args, &(impl->filter_graph), &(impl->buffersrc_ctx), &(impl->buffersink_ctx))) {
  //   return nullptr;
  // }

  auto demux_ctx = std::make_unique<DemuxContext>();
  demux_ctx->params = params;
  demux_ctx->info = video_info;
  demux_ctx->impl = std::move(impl);
  return demux_ctx;
}

auto DemuxSeek(const DemuxContext &demux_ctx,
  const int frame_nb,
  DemuxFrame *const demux_frame) noexcept -> bool
{
  const auto *impl = demux_ctx.impl.get();
  assert(impl != nullptr);// NOLINT

  const auto filter_func = [&](AVFrame *dec_frame) noexcept {
    if (!FilterFrame(impl->buffersrc_ctx, impl->buffersink_ctx, dec_frame, impl->filt_frame)) {
      // av_frame_unref(impl->dec_frame);
      av_frame_unref(impl->filt_frame);
      return false;
    }
    assert(dec_frame->pts == impl->filt_frame->pts);// NOLINT
    // av_frame_unref(impl->dec_frame);

    // Transfer frame data to output frame.
    demux_frame->width = impl->filt_frame->width;
    demux_frame->height = impl->filt_frame->height;
    demux_frame->frame_nb = frame_nb;
    demux_frame->key_frame = impl->filt_frame->key_frame > 0;
    demux_frame->pixel_aspect_ratio = 1.0;
    if (!(impl->dec_frame->sample_aspect_ratio.num == 0
          && impl->dec_frame->sample_aspect_ratio.den == 1)) {
      demux_frame->pixel_aspect_ratio = av_q2d(impl->dec_frame->sample_aspect_ratio);
    }

    PixelData<float> pix = {};
    constexpr std::array<Channel, 4> kChannels = {
      Channel::kGreen,
      Channel::kBlue,
      Channel::kRed,
      Channel::kAlpha,
    };
    std::size_t channel_count = 0U;

    // clang-format off
    switch (impl->filt_frame->format) {
    case AV_PIX_FMT_GRAYF32:
      demux_frame->pix_fmt = PixelFormat::kGray;
      channel_count = ChannelCount(demux_frame->pix_fmt);
      demux_frame->buf.count = channel_count;
      demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
      demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
      demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
      pix = ChannelData(demux_frame, Channel::kGray);
      assert(!Empty(pix));// NOLINT
      assert(impl->filt_frame->data[0] != nullptr);// NOLINT
      std::memcpy(pix.data, impl->filt_frame->data[0], pix.count * sizeof(float));// NOLINT
      break;
    case AV_PIX_FMT_GBRPF32:
      demux_frame->pix_fmt = PixelFormat::kRGB;
      channel_count = ChannelCount(demux_frame->pix_fmt);
      demux_frame->buf.count = channel_count;
      demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
      demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
      demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
      for (std::size_t i = 0U; i < channel_count; ++i) {
        pix = ChannelData(demux_frame, kChannels[i]);// NOLINT
        assert(!Empty(pix));// NOLINT
        assert(impl->filt_frame->data[i] != nullptr);// NOLINT
        std::memcpy(pix.data, impl->filt_frame->data[i], pix.count * sizeof(float));// NOLINT
      }
      break;
    case AV_PIX_FMT_GBRAPF32:
      demux_frame->pix_fmt = PixelFormat::kRGBA;
      channel_count = ChannelCount(demux_frame->pix_fmt);
      demux_frame->buf.count = channel_count;
      demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
      demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
      demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
      for (std::size_t i = 0U; i < channel_count; ++i) {
        pix = ChannelData(demux_frame, kChannels[i]);// NOLINT
        assert(!Empty(pix));// NOLINT
        assert(impl->filt_frame->data[i] != nullptr);// NOLINT
        std::memcpy(pix.data, impl->filt_frame->data[i], pix.count * sizeof(float));// NOLINT
      }
      break;
    default:
      LogMsg(LogLevel::kError, "Unsupported pixel format\n");
      return false;
    }
    // clang-format on

    return true;
  };

  return SeekFrame(impl->ifmt_ctx,
    impl->dec_ctx,
    impl->dec_pkt,
    impl->dec_frame,
    impl->video_stream_index,
    frame_nb - 1,
    filter_func);


#if 0
  const auto *impl = demux_ctx.impl.get();
  assert(impl != nullptr);// NOLINT

  assert(0 <= impl->video_stream_index// NOLINT
         && impl->video_stream_index < static_cast<int>(impl->ifmt_ctx->nb_streams));
  const AVStream *video_stream = impl->ifmt_ctx->streams[impl->video_stream_index];// NOLINT
  assert(video_stream != nullptr);// NOLINT

  // Seek is done on frame pts.
  // NOTE: Frame index is zero-based.
  const int64_t target_pts = FrameIndexToTimestamp(frame_nb - 1,
    video_stream->r_frame_rate,
    video_stream->time_base,
    video_stream->start_time != AV_NOPTS_VALUE ? video_stream->start_time : 0);

  if constexpr (kTrace.enabled) {
    {
      std::ostringstream oss;
      oss << "Stream | nb_frames: " << video_stream->nb_frames
          << ", frame_rate: " << video_stream->r_frame_rate.num << "/"
          << video_stream->r_frame_rate.den << ", start_time: " << video_stream->start_time
          << ", time_base: " << video_stream->time_base.num << "/" << video_stream->time_base.den
          << "\n";
      LogMsg(kTrace.log_level, oss.str().c_str());
    }
    {
      std::ostringstream oss;
      oss << "Codec | framerate. " << impl->dec_ctx->framerate.num << "/"
          << impl->dec_ctx->framerate.den << "\n";
      LogMsg(kTrace.log_level, oss.str().c_str());
    }
    {
      std::ostringstream oss;
      oss << "target_pts: " << target_pts << "\n";
      LogMsg(LogLevel::kInfo, oss.str().c_str());
    }
  }

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret =
        av_seek_frame(impl->ifmt_ctx, impl->video_stream_index, target_pts, kSeekFlags);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot seek to timestamp", ret);
    return false;
  }
  avcodec_flush_buffers(impl->dec_ctx);

  bool got_frame = false;
  int ret = 0;
  do {
    {
      // Read a packet, which for video streams corresponds to one frame.
      PacketRefHolder pkt{ impl->dec_pkt };
      ret = av_read_frame(impl->ifmt_ctx, pkt.get());

      // Ignore packets that are not from the video stream we are interested in.
      if (ret >= 0 && pkt.get()->stream_index != impl->video_stream_index) {
        pkt.unref();
        continue;
      }

      if constexpr (kTrace.enabled) {
        log_utils_internal::LogPacket(impl->ifmt_ctx, impl->dec_pkt, ilp_movie::LogLevel::kInfo);
      }

      if (ret < 0) {
        // Failed to read frame - send flush packet to decoder.
        ret = avcodec_send_packet(impl->dec_ctx, /*avpkt=*/nullptr);
      } else {
        if (pkt.get()->pts == AV_NOPTS_VALUE) {
          LogMsg(LogLevel::kError, "Missing packet PTS (B-frame?)\n");
          return false;
        }
        ret = avcodec_send_packet(impl->dec_ctx, pkt.get());
      }

      // Packet has been sent to the decoder. It gets unref'd by the holder at the end of scope.
    }

    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot submit a packet for decoding", ret);
      return false;
    }

    while (ret >= 0) {
      FrameRefHolder frh{ impl->dec_frame };
      ret = avcodec_receive_frame(impl->dec_ctx, frh.get());
      if (ret == AVERROR_EOF) {// NOLINT
        goto end;// should this be an error if we haven't found our seek frame yet!?
      } else if (ret == AVERROR(EAGAIN)) {
        ret = 0;
        break;
      } else if (ret < 0) {
        log_utils_internal::LogAvError("Cannot decode frame", ret);
        return false;
      }

      if constexpr (kTrace.enabled) {
        std::ostringstream oss;
        oss << "Frame | pts: " << impl->dec_frame->pts
            << " | best_effort_ts: " << impl->dec_frame->best_effort_timestamp
            << " | pkt_duration: " << impl->dec_frame->pkt_duration
            << " | key_frame: " << impl->dec_frame->key_frame
            << " | sample_aspect_ratio: " << impl->dec_frame->sample_aspect_ratio.num << "/"
            << impl->dec_frame->sample_aspect_ratio.den << "\n";
        LogMsg(kTrace.log_level, oss.str().c_str());
      }

      impl->dec_frame->pts = impl->dec_frame->best_effort_timestamp;
      if (impl->dec_frame->pts == AV_NOPTS_VALUE) {
        LogMsg(LogLevel::kError, "Missing frame PTS\n");
        return false;
      }

      // Check if the decoded frame has a PTS/duration that matches our seek target. If so, we will
      // push the frame through the filter graph.
      if (impl->dec_frame->pts <= target_pts
          && target_pts < (impl->dec_frame->pts + impl->dec_frame->pkt_duration)) {
        // Got a frame whose duration includes our seek target.

        if (!FilterFrame(
              impl->buffersrc_ctx, impl->buffersink_ctx, impl->dec_frame, impl->filt_frame)) {
          av_frame_unref(impl->dec_frame);
          av_frame_unref(impl->filt_frame);
          return false;
        }
        assert(impl->dec_frame->pts == impl->filt_frame->pts);// NOLINT
        av_frame_unref(impl->dec_frame);

        // Transfer frame data to output frame.
        demux_frame->width = impl->filt_frame->width;
        demux_frame->height = impl->filt_frame->height;
        demux_frame->frame_nb = frame_nb;
        demux_frame->key_frame = impl->filt_frame->key_frame > 0;
        demux_frame->pixel_aspect_ratio = 1.0;
        if (!(impl->dec_frame->sample_aspect_ratio.num == 0
              && impl->dec_frame->sample_aspect_ratio.den == 1)) {
          demux_frame->pixel_aspect_ratio = av_q2d(impl->dec_frame->sample_aspect_ratio);
        }

        PixelData<float> pix = {};
        constexpr std::array<Channel, 4> kChannels = {
          Channel::kGreen,
          Channel::kBlue,
          Channel::kRed,
          Channel::kAlpha,
        };
        std::size_t channel_count = 0U;

        // clang-format off
        switch (impl->filt_frame->format) {
        case AV_PIX_FMT_GRAYF32:
          demux_frame->pix_fmt = PixelFormat::kGray;
          channel_count = ChannelCount(demux_frame->pix_fmt);
          demux_frame->buf.count = channel_count;
          demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
          demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
          demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
          pix = ChannelData(demux_frame, Channel::kGray);
          assert(!Empty(pix));// NOLINT
          assert(impl->filt_frame->data[0] != nullptr);// NOLINT
          std::memcpy(pix.data, impl->filt_frame->data[0], pix.count * sizeof(float));// NOLINT
          break;
        case AV_PIX_FMT_GBRPF32:
          demux_frame->pix_fmt = PixelFormat::kRGB;
          channel_count = ChannelCount(demux_frame->pix_fmt);
          demux_frame->buf.count = channel_count;
          demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
          demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
          demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
          for (std::size_t i = 0U; i < channel_count; ++i) {
            pix = ChannelData(demux_frame, kChannels[i]);// NOLINT
            assert(!Empty(pix));// NOLINT
            assert(impl->filt_frame->data[i] != nullptr);// NOLINT
            std::memcpy(pix.data, impl->filt_frame->data[i], pix.count * sizeof(float));// NOLINT
          }
          break;
        case AV_PIX_FMT_GBRAPF32:
          demux_frame->pix_fmt = PixelFormat::kRGBA;
          channel_count = ChannelCount(demux_frame->pix_fmt);
          demux_frame->buf.count = channel_count;
          demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
          demux_frame->buf.count *= static_cast<size_t>(demux_frame->width);
          demux_frame->buf.data = std::make_unique<float[]>(demux_frame->buf.count);// NOLINT
          for (std::size_t i = 0U; i < channel_count; ++i) {
            pix = ChannelData(demux_frame, kChannels[i]);// NOLINT
            assert(!Empty(pix));// NOLINT
            assert(impl->filt_frame->data[i] != nullptr);// NOLINT
            std::memcpy(pix.data, impl->filt_frame->data[i], pix.count * sizeof(float));// NOLINT
          }
          break;
        default:
          LogMsg(LogLevel::kError, "Unsupported pixel format\n");
          return false;
        }
        // clang-format off

        av_frame_unref(impl->filt_frame);
        got_frame = true;
        goto end;
      }

      // Frame gets unref'd by the holder at the end of scope.
    }

  } while (ret >= 0 && !got_frame);

end:
  return got_frame;
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
