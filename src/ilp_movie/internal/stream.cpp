#include <internal/stream.hpp>

#include <cmath>// std::round
#include <limits>// std::numeric_limits

#include <internal/log_utils.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/buffer.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace ilp_movie::stream_internal {

class StreamImpl
{
public:
  StreamImpl(AVFormatContext *av_format_ctx, AVStream *av_stream, int index, int thread_count);
  ~StreamImpl();

  // Movable.
  StreamImpl(StreamImpl &&rhs) noexcept = default;
  StreamImpl &operator=(StreamImpl &&rhs) noexcept = default;

  // Not copyable.
  StreamImpl(const StreamImpl &rhs) = delete;
  StreamImpl &operator=(const StreamImpl &rhs) = delete;

  [[nodiscard]] auto StreamIndex() const noexcept -> int;

  [[nodiscard]] auto StartPts() const noexcept -> int64_t;
  [[nodiscard]] auto FrameToPts(int frame) const noexcept -> int64_t;
  [[nodiscard]] auto SecondsToPts(double secs) const noexcept -> int64_t;
  [[nodiscard]] auto DurationSeconds() const noexcept -> double;
  [[nodiscard]] auto DurationFrames() const noexcept -> int;

  // If av_packet is nullptr it is considered a 'flush packet'.
  [[nodiscard]] auto SendPacket(AVPacket *av_packet) noexcept -> bool;

  void FlushBuffers() noexcept;

  [[nodiscard]] auto Stream() const noexcept -> AVStream*;

  [[nodiscard]] auto CodecContext() const noexcept -> AVCodecContext*;

private:
  void _Free() noexcept;

  AVFormatContext *_av_format_ctx = nullptr;
  AVStream *_av_stream = nullptr;
  const AVCodec *_av_codec = nullptr;
  AVMediaType _av_codec_type = AVMEDIA_TYPE_UNKNOWN;

  AVFrame *_av_frame = nullptr;
  AVCodecContext *_av_codec_context = nullptr;

  int _index = -1;

  struct
  {
    int64_t num = 0;// Numerator of FPS
    int64_t den = 0;// Denominator of FPS (if fractional)
  } _fps = {};
  frame_rate_internal::FrameRate _frame_rate = frame_rate_internal::FrameRate{};

  StreamType::ValueType _stream_type = StreamType::kUnknown;
};

StreamImpl::StreamImpl(AVFormatContext *const av_format_ctx,
  AVStream *const av_stream,
  const int index,
  const int thread_count)
  : _av_format_ctx{ av_format_ctx }, _av_stream{ av_stream }, _index{ index }
{
  const auto exit_failure = [&]() {
    _Free();
    throw std::invalid_argument{ "" };// msg!?!?!?
  };

  _av_codec_type = _av_stream->codecpar->codec_type;

  _av_codec = avcodec_find_decoder(_av_stream->codecpar->codec_id);
  if (_av_codec == nullptr) {
    log_utils_internal::LogAvError("Cannot find decoder", AVERROR(EINVAL));
    exit_failure();
  }

  _av_codec_context = avcodec_alloc_context3(_av_codec);
  if (_av_codec_context == nullptr) {
    log_utils_internal::LogAvError("Cannot allocate codec context", AVERROR(ENOMEM));
    exit_failure();
  }

  _av_frame = av_frame_alloc();
  if (_av_frame == nullptr) {
    LogMsg(LogLevel::kError, "Cannot allocate frame\n");
    exit_failure();
  }

  if (_av_codec_type == AVMEDIA_TYPE_VIDEO) {
    _stream_type = StreamType::kVideo;

    if (thread_count > 0) {
#if 0// NOTE(tohi): experimental, not tested.
      if (st->codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {// NOLINT
        st->codec_context->thread_count = thread_count;
        st->codec_context->thread_type = FF_THREAD_SLICE;// NOLINT
      }
      if (st->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {// NOLINT
        st->codec_context->thread_count = thread_count;
        st->codec_context->thread_type |= FF_THREAD_FRAME;// NOLINT
      }
#endif
    }

    // Copy decoder parameters to the input stream.
    if (const int ret = avcodec_parameters_to_context(_av_codec_context, _av_stream->codecpar);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot copy stream parameters to the decoder", ret);
      exit_failure();
    }

    // Init the video decoder.
    if (const int ret = avcodec_open2(_av_codec_context, _av_codec, /*options=*/nullptr); ret < 0) {
      log_utils_internal::LogAvError("Cannot open video decoder", ret);
      exit_failure();
    }

    _av_frame->width = _av_stream->codecpar->width;
    _av_frame->height = _av_stream->codecpar->height;
    _av_frame->format = _av_codec_context->pix_fmt;

    _av_codec_context->opaque = this;
  }
  // else if (_codec_type == AVMEDIA_TYPE_AUDIO && st->codec != nullptr) { ... }
  // else if (_codec_type == AVMEDIA_TYPE_DATA) { ... }

  // Set the fps if it has been set correctly in the stream.
  if (_av_stream->avg_frame_rate.num != 0 && _av_stream->avg_frame_rate.den != 0) {
    _fps.num = _av_stream->avg_frame_rate.num;
    _fps.den = _av_stream->avg_frame_rate.den;
    _frame_rate = frame_rate_internal::FrameRate{ static_cast<double>(_fps.den)
                                                  / static_cast<double>(_fps.num) };
  } else {
    _fps.num = 0;
    _fps.den = 0;
    _frame_rate = frame_rate_internal::FrameRate{ timebase::k_flicks_24fps };
  }
}

StreamImpl::~StreamImpl() { _Free(); }

auto StreamImpl::StreamIndex() const noexcept -> int { return _index; }

auto StreamImpl::StartPts() const noexcept -> int64_t
{
  int64_t start_pts = _av_stream->start_time;
  if (start_pts == AV_NOPTS_VALUE) {
#if 0
    // NOTE(tohi): This is the try-hard version where we start reading frames to figure out the 
    //             first PTS. 

    // Seek first key-frame in video stream.
    avcodec_flush_buffers(st.codec_context);
    if (av_seek_frame(st.format_context, st.stream_index, /*timestamp=*/0, /*flags=*/0) >= 0) {
      AVPacket *pkt = av_packet_alloc();
      do {
        // Read frames until we get one for the video stream that contains a valid PTS.
        if (av_read_frame(st.format_context, pkt) < 0) {
          // Read error or EOF. Abort search for PTS.
          break;
        }
        if (pkt->stream_index == st.stream_index) {
          // Packet read for video stream. Get its PTS. Loop will continue if the PTS is
          // still invalid (AV_NOPTS_VALUE).
          start_pts = pkt->pts;
        }
        av_packet_unref(pkt);
      } while (start_pts == AV_NOPTS_VALUE);
      av_packet_free(&pkt);
    }
    // else: seek error!

    // If we still don't have a valid initial PTS, assume 0. (This really shouldn't happen for any
    // real media file, as it would make meaningful playback presentation timing and seeking
    // impossible)
    if (start_pts == AV_NOPTS_VALUE) { start_pts = 0; }
#else
    // NOTE(tohi): This is the simple version where we assume the first PTS to be zero if not given.
    //             (this is how it is handled in xStudio)

    start_pts = 0;
#endif
  }

  return start_pts;
}

auto StreamImpl::FrameToPts(const int frame) const noexcept -> int64_t
{
  return SecondsToPts(static_cast<double>(frame) * _frame_rate.to_seconds());
}

auto StreamImpl::SecondsToPts(const double secs) const noexcept -> int64_t
{
  return _av_stream->time_base.num == 0
           ? 0
           : static_cast<int64_t>(secs * static_cast<double>(_av_stream->time_base.den)
                                  / static_cast<double>(_av_stream->time_base.num));
}

auto StreamImpl::DurationSeconds() const noexcept -> double
{
  if (_av_stream->time_base.num != 0
      && _av_stream->duration != std::numeric_limits<int64_t>::lowest()) {
    return static_cast<double>(_av_stream->duration * _av_stream->time_base.num)
           / static_cast<double>(_av_stream->time_base.den);
  } else {
    // Stream duration is not known. Applying some rules based on trial and
    // error investigation.
    if (_av_format_ctx->duration > 0
        && _av_stream->duration == std::numeric_limits<int64_t>::lowest()) {
      // I've found that on MKV/VP8 (webm) sources, if duration is set on the
      // AVFormatContext it is stated in microseconds (AV_TIME_BASE), not in the timebase
      // of the AVStream. Undocumented behaviour in ffmpeg, of course.
      return static_cast<double>(_av_format_ctx->duration) * 0.000001;
    } else if (_av_stream->nb_frames > 0 && _av_stream->avg_frame_rate.num != 0) {
      // maybe we know the number of frames and frame rate
      return static_cast<double>(_av_stream->nb_frames * _av_stream->avg_frame_rate.den)
             / static_cast<double>(_av_stream->avg_frame_rate.num);
    } else if (_av_stream->time_base.num != 0) {
      // single frame source
      return static_cast<double>(_av_stream->time_base.num)
             / static_cast<double>(_av_stream->time_base.den);
    }
  }
  // fallback, assume single frame source
  return _frame_rate.to_seconds();
}

auto StreamImpl::DurationFrames() const noexcept -> int
{
  if (_stream_type == StreamType::kVideo && _fps.den == 0) {
    // No average frame rate defined, assume it's a single pic.
    return 1;
  }
  return static_cast<int>(std::round(DurationSeconds() / _frame_rate.to_seconds()));
}

auto StreamImpl::SendPacket(AVPacket *const av_packet) noexcept -> bool
{
  bool success = true;
  if (const int ret = avcodec_send_packet(_av_codec_context, av_packet); ret < 0) {
    log_utils_internal::LogAvError("Cannot send packet to decoder", ret);
    success = false;
  }

  if (av_packet != nullptr) { av_packet_unref(av_packet); }
  return success;
}

// auto StreamImpl::ReceiveFrame() noexcept -> int
// {
//   // // for single frame video source, check if the frame has already been
//   // // decoded
//   // if (duration_frames() == 1 && !nothing_decoded_yet_) { return AVERROR_EOF; }

//   // av_frame_unref(_av_frame);
//   // const int ret = avcodec_receive_frame(_av_codec_context, _av_frame);

//   // int rt = avcodec_receive_frame(codec_context_, frame);

//   // // we have decoded a frame, increment the frame counter
//   // // if our frame counter is set.
//   // if (rt != 0 && current_frame_ != CURRENT_FRAME_UNKNOWN) current_frame_++;

//   // if (rt != AVERROR(EAGAIN) && rt != AVERROR_EOF) { AVC_CHECK_THROW(rt, "avcodec_receive_frame"); }

//   // if (rt != 0 && nothing_decoded_yet_) nothing_decoded_yet_ = false;

//   // // no error, but might need more packet data to receive aother frame or we've
//   // // hit the end of the file

//   // return rt;
//   return -1;
// }

void StreamImpl::FlushBuffers() noexcept { avcodec_flush_buffers(_av_codec_context); }

auto StreamImpl::Stream() const noexcept -> AVStream* {
  return _av_stream;
}

auto StreamImpl::CodecContext() const noexcept -> AVCodecContext* {
  return _av_codec_context;
}

void StreamImpl::_Free() noexcept
{
  if (_av_frame != nullptr) {
    av_frame_unref(_av_frame);
    av_frame_free(&_av_frame);
  }
  if (_av_codec_context != nullptr) { avcodec_free_context(&_av_codec_context); }
}


// -----------

Stream::Stream(AVFormatContext *const av_format_ctx,
  AVStream *const av_stream,
  const int index,
  const int thread_count)
  : _pimpl{ std::make_unique<StreamImpl>(av_format_ctx, av_stream, index, thread_count) }
{}

Stream::~Stream() = default;

auto Stream::StreamIndex() const noexcept -> int { return Pimpl()->StreamIndex(); }

auto Stream::StartPts() const noexcept -> int64_t { return Pimpl()->StartPts(); }

auto Stream::FrameToPts(const int frame) const noexcept -> int64_t
{
  return Pimpl()->FrameToPts(frame);
}

auto Stream::SecondsToPts(const double secs) const noexcept -> int64_t
{
  return Pimpl()->SecondsToPts(secs);
}

auto Stream::DurationSeconds() const noexcept -> double { return Pimpl()->DurationSeconds(); }

auto Stream::DurationFrames() const noexcept -> int { return Pimpl()->DurationFrames(); }

auto Stream::SendPacket(AVPacket *const av_packet) noexcept -> bool
{
  return Pimpl()->SendPacket(av_packet);
}

auto Stream::RawStream() const noexcept -> AVStream* {
  return Pimpl()->Stream();
}

auto Stream::SendFlushPacket() noexcept -> bool { return SendPacket(nullptr); }

void Stream::FlushBuffers() noexcept { Pimpl()->FlushBuffers(); }

auto Stream::CodecContext() const noexcept -> AVCodecContext* {
  return Pimpl()->CodecContext();
}
}// namespace ilp_movie::stream_internal


#if 0
// Returns the stream duration as number of frames.
[[nodiscard]] static auto StreamFrameCount(AVFormatContext *const fmt_ctx,
  AVCodecContext *const dec_ctx,
  const int video_stream_index,
  const int64_t start_pts) noexcept -> int64_t
{
  int64_t frame_count = 0;
  const AVRational frame_rate = StreamFrameRate(fmt_ctx, video_stream_index);
  const AVStream *video_stream = fmt_ctx->streams[video_stream_index];// NOLINT

  // Obtain from movie duration if specified. This is preferred since mov/mp4 formats allow the
  // media in tracks (=streams) to be remapped in time to the final movie presentation without
  // needing to recode the underlying tracks content; the movie duration thus correctly describes
  // the final presentation.
  if (fmt_ctx->duration != 0) {
    // Annoyingly, FFmpeg exposes the movie duration converted (with round-to-nearest semantics) to
    // units of AV_TIME_BASE (microseconds in practice) and does not expose the original rational
    // number duration from a mov/mp4 file's "mvhd" atom/box. Accuracy may be lost in this
    // conversion; a duration that was an exact number of frames as a rational may end up as a
    // duration slightly over or under that number of frames in units of AV_TIME_BASE. Conversion to
    // whole frames rounds up the resulting number of frames because a partial frame is still a
    // frame. However, in an attempt to compensate for AVFormatContext's inaccurate representation
    // of duration, with unknown rounding direction, the conversion to frames subtracts 1 unit
    // (microsecond) from that duration. The rationale for this is thus:
    // * If the stored duration exactly represents an exact number of frames, then that duration
    //   minus 1 will result in that same number of frames once rounded up.
    // * If the stored duration is for an exact number of frames that was rounded down, then that
    //   duration minus 1 will result in that number of frames once rounded up.
    // * If the stored duration is for an exact number of frames that was rounded up, then that
    //   duration minus 1 will result in that number of frames once rounded up, while that duration
    //   unchanged would result in 1 more frame being counted after rounding up.
    // * If the original duration in the file was not for an exact number of frames, then the movie
    //   timebase would have to be >= 10^6 for there to be any chance of this calculation resulting
    //   in the wrong number of frames. This isn't a case that I've seen. Even if that were to be
    //   the case, the original duration would have to be <= 1 microsecond greater than an exact
    //   number of frames in order to result in the wrong number of frames, which is highly
    //   improbable.

    int64_t divisor = int64_t(AV_TIME_BASE) * frame_rate.den;
    frame_count = ((fmt_ctx->duration - 1) * frame_rate.num + divisor - 1) / divisor;

    // The above calculation is not reliable, because it seems in some situations (such as rendering
    // out a mov with 5 frames at 24 fps from Nuke) the duration has been rounded up to the nearest
    // millisecond, which leads to an extra frame being reported.  To attempt to work around this,
    // compare against the number of frames in the stream, and if they differ by one, use that value
    // instead.
    int64_t stream_frames = video_stream->nb_frames;
    if ((stream_frames > 0) && (std::abs(static_cast<double>(frame_count - stream_frames)) <= 1)) {
      frame_count = stream_frames;
    }
  }

  // If number of frames still unknown, obtain from stream's number of frames if specified. Will be
  // 0 if unknown.
  if (frame_count == 0) { frame_count = video_stream->nb_frames; }

  // If number of frames still unknown, attempt to calculate from stream's duration, fps and
  // timebase.
  if (frame_count == 0) {
    frame_count =
      (static_cast<int64_t>(video_stream->duration) * video_stream->time_base.num * frame_rate.num)
      / (static_cast<int64_t>(video_stream->time_base.den) * frame_rate.den);
  }

  // If the number of frames is still unknown, attempt to measure it from the last frame PTS for the
  // stream in the file relative to first (which we know from earlier).
  if (frame_count == 0) {
    int64_t maxPts = start_pts;


    timestamp_internal::FrameToPts()

    // Seek last key-frame.
    avcodec_flush_buffers(dec_ctx);
    av_seek_frame(fmt_ctx, video_stream_index, stream.frameToPts(1 << 29), AVSEEK_FLAG_BACKWARD);

    // Read up to last frame, extending max PTS for every valid PTS value found for the video
    // stream.
    av_init_packet(&_avPacket);

    while (av_read_frame(_context, &_avPacket) >= 0) {
      if ((_avPacket.stream_index == stream._idx) && (_avPacket.pts != int64_t(AV_NOPTS_VALUE))
          && (_avPacket.pts > maxPts)) {
        maxPts = _avPacket.pts;
      }
      av_packet_unref(&_avPacket);
    }
#if TRACE_FILE_OPEN
    std::cout << "          Start PTS=" << stream._startPTS << ", Max PTS found=" << maxPts
              << std::endl;
#endif

    // Compute frame range from min to max PTS. Need to add 1 as both min and max are at starts of
    // frames, so stream extends for 1 frame beyond this.
    frames = 1 + stream.ptsToFrame(maxPts);
#if TRACE_FILE_OPEN
    std::cout << "        Calculated from frame PTS range=";
#endif
  }

#if TRACE_FILE_OPEN
  std::cout << frames << std::endl;
#endif

  return frame_count;
}
#endif


#if 0
[[nodiscard]] static auto StreamStartPts(AVFormatContext *const fmt_ctx,
  AVCodecContext *const dec_ctx,
  const int video_stream_index) noexcept -> int64_t
{
  // Read from stream. If the value read isn't valid, get it from the first frame in the stream that
  // provides such a value.
  const AVStream *video_stream = fmt_ctx->streams[video_stream_index];// NOLINT
  int64_t start_pts = video_stream->start_time;

  if (start_pts == AV_NOPTS_VALUE) {
    // Seek first key-frame in video stream.
    avcodec_flush_buffers(dec_ctx);
    if (av_seek_frame(fmt_ctx, video_stream_index, /*timestamp=*/0, /*flags=*/0) >= 0) {
      AVPacket *pkt = av_packet_alloc();
      do {
        // Read frames until we get one for the video stream that contains a valid PTS.
        if (av_read_frame(fmt_ctx, pkt) < 0) {
          // Read error or EOF. Abort search for PTS.
          break;
        }
        if (pkt->stream_index == video_stream_index) {
          // Packet read for video stream. Get its PTS. Loop will continue if the PTS is
          // still invalid (AV_NOPTS_VALUE).
          start_pts = pkt->pts;
        }
        av_packet_unref(pkt);
      } while (start_pts == AV_NOPTS_VALUE);
      av_packet_free(&pkt);
    }
    // else: seek error!
  }

  // If we still don't have a valid initial PTS, assume 0. (This really shouldn't happen for any
  // real media file, as it would make meaningful playback presentation timing and seeking
  // impossible)
  if (start_pts == AV_NOPTS_VALUE) { start_pts = 0; }

  return start_pts;
}
#endif
