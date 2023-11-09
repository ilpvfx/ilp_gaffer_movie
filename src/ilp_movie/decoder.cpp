#include "ilp_movie/decoder.hpp"

#include <cassert>
#include <cstdint>// int64_t
#include <cstring>// std::memcpy
#include <map>//std::map

#include <ilp_movie/pixel_data.hpp>
#include <internal/filter_graph.hpp>
#include <internal/log_utils.hpp>
#include <internal/stream.hpp>

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

namespace {

class Stream
{
public:
  explicit Stream(AVFormatContext *av_fmt_ctx,
    AVStream *const av_stream,
    const int thread_count = 0)
    : _av_stream(av_stream)
  {
    const auto exit_failure = [&]() {
      _Free();
      throw std::invalid_argument{ "Stream::Stream()" };// msg!?!?!?
    };

    assert(_av_stream != nullptr);// NOLINT

    const AVCodec *codec = avcodec_find_decoder(_av_stream->codecpar->codec_id);
    if (codec == nullptr) {
      log_utils_internal::LogAvError("Cannot find decoder", AVERROR(EINVAL));
      exit_failure();
    }

    _av_codec_context = avcodec_alloc_context3(codec);
    if (_av_codec_context == nullptr) {
      log_utils_internal::LogAvError("Cannot allocate codec context", AVERROR(ENOMEM));
      exit_failure();
    }

    if (_av_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (thread_count > 0) {
#if 0// NOTE(tohi): experimental, not tested.
        if (st->codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {// NOLINT
          _av_codec_context->thread_count = thread_count;
          _av_codec_context->thread_type = FF_THREAD_SLICE;// NOLINT
        }
        if (st->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {// NOLINT
          _av_codec_context->thread_count = thread_count;
          _av_codec_context->thread_type |= FF_THREAD_FRAME;// NOLINT
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
      if (const int ret = avcodec_open2(_av_codec_context, codec, /*options=*/nullptr); ret < 0) {
        log_utils_internal::LogAvError("Cannot open video decoder", ret);
        exit_failure();
      }

#if 0
      _av_codec_context->opaque = this;// Why?
#endif
    } else {
      ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Only video streams supported\n");
      exit_failure();
    }

    _start_time = _av_stream->start_time;
    if (_start_time == AV_NOPTS_VALUE) {
      // NOTE(tohi): This is the simple version where we assume the first PTS to be zero if not
      //             given. (this is how it is handled in xStudio)
      ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Guessing stream start time to be 0\n");
      _start_time = 0;
    }

    _frame_count = _av_stream->nb_frames;
    if (_frame_count == 0) {
      ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Guessing stream frame count to be 1\n");
      _frame_count = 1;
    }

#if 1
    _frame_rate = av_guess_frame_rate(av_fmt_ctx, _av_stream, /*frame=*/nullptr);
    if (_frame_rate.num == 0 && _frame_rate.den == 1) {
      ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Guessing stream frame rate to be 24/1\n");
      _frame_rate.num = 24;
      _frame_rate.den = 1;
    }
#else
    // Set the fps if it has been set correctly in the stream.
    _frame_rate = _av_stream->avg_frame_rate;
    if (!(_frame_rate.num != 0 && _frame_rate.den != 0)) {
      ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Guessing stream frame rate to be 24/1\n");
      _frame_rate.num = 24;
      _frame_rate.den = 1;
    }
#endif
  }

  ~Stream() { _Free(); }

  // Movable.
  Stream(Stream &&rhs) noexcept = default;
  Stream &operator=(Stream &&rhs) noexcept = default;

  // Not copyable.
  Stream(const Stream &rhs) = delete;
  Stream &operator=(const Stream &rhs) = delete;

  [[nodiscard]] auto Get() const noexcept -> AVStream *
  {
    assert(_av_stream != nullptr);// NOLINT
    return _av_stream;
  }

  [[nodiscard]] auto CodecContext() const noexcept -> AVCodecContext *
  {
    assert(_av_codec_context != nullptr);// NOLINT
    return _av_codec_context;
  }

  [[nodiscard]] auto Index() const noexcept -> int
  {
    assert(_av_stream->index >= 0);// NOLINT
    return _av_stream->index;
  }

  [[nodiscard]] auto Width() const noexcept -> int
  {
    assert(_av_stream->codecpar->width > 0);// NOLINT
    return _av_stream->codecpar->width;
  }

  [[nodiscard]] auto Height() const noexcept -> int
  {
    assert(_av_stream->codecpar->height > 0);// NOLINT
    return _av_stream->codecpar->height;
  }

  [[nodiscard]] auto FrameRate() const noexcept -> AVRational { return _frame_rate; }

  // [PTS].
  [[nodiscard]] auto StartTime() const noexcept -> int64_t { return _start_time; }

  [[nodiscard]] auto FrameCount() const noexcept -> int64_t { return _frame_count; }

  [[nodiscard]] auto FrameToPts(const int frame_index) const noexcept -> int64_t
  {
    const int64_t num =
      static_cast<int64_t>(frame_index) * _frame_rate.den * _av_stream->time_base.den;
    const int64_t den = static_cast<int64_t>(_frame_rate.num) * _av_stream->time_base.num;
    return _start_time + (den > 0 ? (num / den) : num);
  }

private:
  void _Free() noexcept
  {
    if (_av_codec_context != nullptr) { avcodec_free_context(&_av_codec_context); }
  }

  AVStream *_av_stream = nullptr;
  AVCodecContext *_av_codec_context = nullptr;

  int64_t _start_time = 0;
  int64_t _frame_count = 0;
  AVRational _frame_rate = { /*.num=*/0, /*.den=*/1 };
};

}// namespace

namespace ilp_movie {

class DecoderImpl
{
public:
  explicit DecoderImpl(std::string path);
  ~DecoderImpl();

  DecoderImpl(DecoderImpl &&rhs) noexcept = default;
  DecoderImpl &operator=(DecoderImpl &&rhs) noexcept = default;

  // Not copyable.
  DecoderImpl(const DecoderImpl &rhs) = delete;
  DecoderImpl &operator=(const DecoderImpl &rhs) = delete;

  [[nodiscard]] auto StreamInfo() const noexcept -> std::vector<DecodeVideoStreamInfo>;

  [[nodiscard]] auto SetStreamFilterGraph(int stream_index, std::string filter_descr) noexcept
    -> bool;

  [[nodiscard]] auto
    DecodeVideoFrame(int stream_index, int frame_nb, DecodedVideoFrame &dvf) noexcept -> bool;

private:
  void _Free() noexcept;

  std::string _path;

  AVFormatContext *_av_format_ctx = nullptr;
  AVPacket *_av_packet = nullptr;
  AVFrame *_av_frame = nullptr;

  int _best_video_stream = -1;

  std::map<int, std::unique_ptr<Stream>> _streams;
  std::map<int, std::unique_ptr<filter_graph_internal::FilterGraph>> _filter_graphs;
  // std::vector<DecodeVideoStreamInfo> _stream_info;
  //  int _decode_stream = -1;
};

DecoderImpl::DecoderImpl(std::string path) : _path{ std::move(path) }
{
  const auto exit_failure = [&]() {
    _Free();
    throw std::invalid_argument{ "DecoderImpl::DecoderImpl" };// msg!?!?!?
  };

  _av_packet = av_packet_alloc();
  if (_av_packet == nullptr) {
    log_utils_internal::LogAvError("Cannot allocate packet", AVERROR(ENOMEM));
    exit_failure();
  }

  _av_frame = av_frame_alloc();
  if (_av_frame == nullptr) {
    log_utils_internal::LogAvError("Cannot allocate frame", AVERROR(ENOMEM));
    exit_failure();
  }

  if (const int ret =
        avformat_open_input(&_av_format_ctx, _path.c_str(), /*fmt=*/nullptr, /*options=*/nullptr);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot open input file", ret);
    exit_failure();
  }

  if (const int ret = avformat_find_stream_info(_av_format_ctx, /*options=*/nullptr); ret < 0) {
    log_utils_internal::LogAvError("Cannot find stream information", ret);
    exit_failure();
  }

  _av_format_ctx->flags |= AVFMT_FLAG_GENPTS;// NOLINT

  _best_video_stream = av_find_best_stream(_av_format_ctx,
    AVMEDIA_TYPE_VIDEO,
    /*wanted_stream_nb=*/-1,
    /*related_stream=*/-1,
    /*decoder_ret=*/nullptr,
    /*flags=*/0);
  if (_best_video_stream < 0) {
    log_utils_internal::LogAvError("Cannot find best video stream", _best_video_stream);
    exit_failure();
  }

  // NOTE(tohi): Not using multi-threading for now.
  constexpr int kThreadCount = 0;

  for (unsigned int i = 0; i < _av_format_ctx->nb_streams; ++i) {
    AVStream *si = _av_format_ctx->streams[i];// NOLINT
    if (si->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      try {
        _streams[si->index] = std::make_unique<Stream>(_av_format_ctx, si, kThreadCount);
      } catch (std::invalid_argument &e) {
        // todo!!!
        // re-throw!?
      }
    }
  }


#if 0
  // if we aren't loading a specific (known) stream, we load all streams to get
  // the details on the source. We also try and find out which video and/or
  // audio tracks are the 'best'.
  int best_video_stream = stream_id_.empty() ? av_find_best_stream(_av_format_ctx,
                            AVMEDIA_TYPE_VIDEO,
                            /*wanted_stream_nb=*/-1,
                            /*related_stream=*/-1,
                            /*decoder_ret=*/nullptr,
                            /*flags=*/0)
                                             : -1;


  StreamPtr primary_video_stream;

  for (auto &s : streams_) {
    auto &stream = s.second;
    if (stream->codec_type() == AVMEDIA_TYPE_VIDEO && !primary_video_stream) {
      if (best_video_stream == -1 || stream->stream_index() == best_video_stream) {
        primary_video_stream = stream;
      }
    }
  }
  else if (stream->stream_type() == TIMECODE_STREAM && !timecode_stream_)
  {
    timecode_stream_ = stream;
  }


  if (primary_video_stream && !primary_video_stream->is_single_frame()) {

    // xSTUDIO requires that streams within a given source have the same frame rate.
    // We force other streams to have a frame rate that matches the 'primary' video
    // stream. Particulary for audio streams, which typically deliver frames at a
    // different rate to video, we then re-parcel audio samples in a sort-of pull-down
    // to match the video rate. See the note immediately below for a bit more detail.
    for (auto &stream : streams_) {

      stream.second->set_virtual_frame_rate(primary_video_stream->frame_rate());
    }

  } else if (primary_audio_stream) {

    // What's happening here is for any audio only sources we enforce a frame rate of 60fps.
    // We do this because ffmpeg doesn't usually tell us the duration of audio frames (and
    // therefore the frame rate) until we start decoding. Audio is really a 'stream' and we
    // can't even be sure that the audio frame durations are constant. Thus, we force the
    // frame rate here. During decode we resolve xSTUDIO's virtual frame number into the
    // actual position in the audio stream. Audio frames coming from ffmpeg are assigned to
    // virtual frames as accurately as possible, but there will be a mismatch between the
    // PTS (presentation time stamp - when the samples should be played) of the ffmpeg audio
    // frame and the PTS of the xSTUDIO frame. We deal this by recording the offset between
    // the ffmpeg PTS and the xSTUDIO PTS. The audio playback engine will take this into
    // account as it streams audio frames to the soundcard.
    for (auto &stream : streams_) {

      stream.second->set_virtual_frame_rate(
        utility::FrameRate(timebase::k_flicks_one_sixtieth_second));
    }
  }

  duration_frames_ = 0;
  if (primary_video_stream) {

    if (primary_audio_stream && primary_video_stream->is_single_frame()) {
      // this can be the case with audio files that have a single image
      // video stream (album cover art, for example) so we fall back on
      // the audio stream duration
      duration_frames_ = primary_audio_stream->duration_frames();
    } else {
      duration_frames_ = primary_video_stream->duration_frames();
    }

  } else if (primary_audio_stream) {
    duration_frames_ = primary_audio_stream->duration_frames();
  }

  if (stream_id_ != "") {
    for (auto p = streams_.begin(); p != streams_.end();) {
      if (make_stream_id(p->first) == stream_id_) {
        decode_stream_ = p->second;
        p++;
      } else {
        p = streams_.erase(p);
      }
    }
  }
#endif
}

DecoderImpl::~DecoderImpl() { _Free(); }

auto DecoderImpl::StreamInfo() const noexcept -> std::vector<DecodeVideoStreamInfo>
{
  std::vector<DecodeVideoStreamInfo> r;
  for (auto &&[index, stream] : _streams) {
    assert(index == stream->Index());// NOLINT
    DecodeVideoStreamInfo dvsi{};
    dvsi.stream_index = stream->Index();
    dvsi.is_best = (dvsi.stream_index == _best_video_stream);
    dvsi.width = stream->Width();
    dvsi.height = stream->Height();
    dvsi.frame_rate.num = stream->FrameRate().num;
    dvsi.frame_rate.den = stream->FrameRate().den;
    dvsi.first_frame_nb = 1;// Good??
    dvsi.frame_count = stream->FrameCount();
    dvsi.pix_fmt_name = av_get_pix_fmt_name(stream->CodecContext()->pix_fmt);
    dvsi.color_range_name = av_color_range_name(stream->CodecContext()->color_range);
    dvsi.color_space_name = av_color_space_name(stream->CodecContext()->colorspace);
    dvsi.color_primaries_name = av_color_primaries_name(stream->CodecContext()->color_primaries);
    r.push_back(dvsi);
  }
  return r;
}

auto DecoderImpl::SetStreamFilterGraph(const int stream_index, std::string filter_descr) noexcept
  -> bool
{
  const auto st_iter = _streams.find(stream_index);
  if (st_iter == _streams.end()) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Bad stream index for filter graph\n");
    return false;
  }
  Stream *st = st_iter->second.get();

  filter_graph_internal::FilterGraphArgs fg_args{};
  fg_args.filter_descr = std::move(filter_descr);
  ;
  fg_args.sws_flags = "";
  fg_args.in.width = st->CodecContext()->width;
  fg_args.in.height = st->CodecContext()->height;
  fg_args.in.pix_fmt = st->CodecContext()->pix_fmt;
  fg_args.in.sample_aspect_ratio = st->CodecContext()->sample_aspect_ratio;
  fg_args.in.time_base = st->Get()->time_base;
  fg_args.out.pix_fmt = AV_PIX_FMT_GBRPF32;// TODO(tohi): is this always good?

  try {
    _filter_graphs[stream_index] = std::make_unique<filter_graph_internal::FilterGraph>(fg_args);
  } catch (std::exception &) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Failed constructing filter graph\n");
    return false;
  }

  return true;
}

auto DecoderImpl::DecodeVideoFrame(int stream_index, int frame_nb, DecodedVideoFrame &dvf) noexcept
  -> bool
{
  Stream *st = nullptr;
  if (const auto st_iter = _streams.find(stream_index); st_iter != _streams.end()) {
    st = st_iter->second.get();
  } else {
    // TODO: log warning
    return false;
  }
  assert(st != nullptr);// NOLINT

  filter_graph_internal::FilterGraph *filter_graph = nullptr;
  if (const auto fg_iter = _filter_graphs.find(stream_index); fg_iter != _filter_graphs.end()) {
    filter_graph = fg_iter->second.get();
  }

  const int64_t timestamp = st->FrameToPts(frame_nb - 1);

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret = av_seek_frame(_av_format_ctx, st->Index(), timestamp, kSeekFlags); ret < 0) {
    log_utils_internal::LogAvError("Cannot seek to timestamp", ret);
    return false;
  }

  avcodec_flush_buffers(st->CodecContext());


  bool got_frame = false;// TODO??!?!?
  int ret = 0;
  do {
    // Read a packet, which for video streams corresponds to one frame.
    ret = av_read_frame(_av_format_ctx, _av_packet);

    // A packet was successfully read, check if was from the stream we are interested in.
    if (ret >= 0 && _av_packet->stream_index != st->Index()) {
      // Ignore packets that are not from the video stream we are trying to read from.
      av_packet_unref(_av_packet);
      continue;
    }

    if (ret < 0) {
      // Failed to read frame/packet - send flush packet to decoder.
      ret = avcodec_send_packet(st->CodecContext(), /*avpkt=*/nullptr);
    } else {
      assert(_av_packet->pts != AV_NOPTS_VALUE);// NOLINT
      // if (_av_packet->pts == AV_NOPTS_VALUE) {
      //   ilp_movie::LogMsg(ilp_movie::LogLevel::kError, "Missing packet PTS (B-frame?)\n");
      //   return false;
      // }
      ret = avcodec_send_packet(st->CodecContext(), _av_packet);
    }

    // Packet has been sent to the decoder.
    av_packet_unref(_av_packet);
    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot send packet to decoder", ret);
      return false;
    }

    while (ret >= 0) {
      ret = avcodec_receive_frame(st->CodecContext(), _av_frame);
      if (ret == AVERROR_EOF) {// NOLINT
        goto end;// should this be an error if we haven't found our seek frame yet!?
      } else if (ret == AVERROR(EAGAIN)) {
        // Break to packet read (outer) loop, send more packets to the decoder.
        ret = 0;
        break;
      } else if (ret < 0) {
        // Legitimate decoding errors.
        log_utils_internal::LogAvError("Cannot decode frame", ret);
        // frame unref?
        return false;
      }
      assert(ret >= 0);// NOLINT

      _av_frame->pts = _av_frame->best_effort_timestamp;

      if (filter_graph != nullptr) {
        if (!filter_graph->FilterFrame(_av_frame, [&](AVFrame *filt_frame) {
              // Check if the frame has a PTS/duration that matches our seek target.
              if (filt_frame->pts <= timestamp
                  && timestamp < (filt_frame->pts + filt_frame->pkt_duration)) {
                dvf.width = filt_frame->width;
                dvf.height = filt_frame->height;
                dvf.key_frame = filt_frame->key_frame > 0;
                dvf.frame_nb = frame_nb;
                dvf.pixel_aspect_ratio = (filt_frame->sample_aspect_ratio.num == 0
                                           && filt_frame->sample_aspect_ratio.den == 1)
                                           ? 1.0
                                           : av_q2d(filt_frame->sample_aspect_ratio);

                dvf.pix_fmt = PixelFormat::kNone;
                switch (filt_frame->format) {
                case AV_PIX_FMT_GRAYF32:
                  dvf.pix_fmt = PixelFormat::kGray;
                  break;
                case AV_PIX_FMT_GBRPF32:
                  dvf.pix_fmt = PixelFormat::kRGB;
                  break;
                case AV_PIX_FMT_GBRAPF32:
                  dvf.pix_fmt = PixelFormat::kRGBA;
                  break;
                default:
                  LogMsg(LogLevel::kError, "Unsupported pixel format\n");
                  return false;
                }
                constexpr std::array<Channel, 4> kChannels = {
                  Channel::kGreen,
                  Channel::kBlue,
                  Channel::kRed,
                  Channel::kAlpha,
                };

                const std::size_t channel_count = ChannelCount(dvf.pix_fmt);
                assert(0U < channel_count && channel_count < 4U);// NOLINT
                assert(dvf.width > 0 && dvf.height > 0);// NOLINT

                dvf.buf.count = channel_count * static_cast<std::size_t>(dvf.width * dvf.height);
                dvf.buf.data = std::make_unique<float[]>(dvf.buf.count);// NOLINT
                for (std::size_t i = 0U; i < channel_count; ++i) {
                  const auto ch = channel_count > 1U ? kChannels.at(i) : Channel::kGray;
                  auto pix = ChannelData(&dvf, ch);// NOLINT
                  assert(!Empty(pix));// NOLINT
                  assert(filt_frame->data[i] != nullptr);// NOLINT
                  std::memcpy(pix.data, filt_frame->data[i], pix.count * sizeof(float));// NOLINT
                }
                got_frame = true;
              }
              return true;
            })) {
          return false;
        }
      } else {
      }

      // _av_frame->decode_error_flags

      // _av_frame->crop_top
      // _av_frame->crop_bottom
      // _av_frame->crop_left
      // _av_frame->crop_right
      // av_frame_apply_cropping(...)
#if 0
      // Check if the decoded frame has a PTS/duration that matches our seek target. If so, we will
      // push the frame through the filter graph.
      _av_frame->pts = _av_frame->best_effort_timestamp;
      if (_av_frame->pts <= timestamp && timestamp < (_av_frame->pts + _av_frame->pkt_duration)) {
        // TODO: push through filter graph!
        got_frame = true;

        dvf.width = _av_frame->width;
        dvf.height = _av_frame->height;
        dvf.key_frame = _av_frame->key_frame > 0;
        dvf.frame_nb = frame_nb;
        dvf.pixel_aspect_ratio =
          (_av_frame->sample_aspect_ratio.num == 0 && _av_frame->sample_aspect_ratio.den == 1)
            ? 1.0
            : av_q2d(_av_frame->sample_aspect_ratio);

        dvf.pix_fmt = PixelFormat::kNone;
        switch (_av_frame->format) {
        case AV_PIX_FMT_GRAYF32:
          dvf.pix_fmt = PixelFormat::kGray;
          break;
        case AV_PIX_FMT_GBRPF32:
          dvf.pix_fmt = PixelFormat::kRGB;
          break;
        case AV_PIX_FMT_GBRAPF32:
          dvf.pix_fmt = PixelFormat::kRGBA;
          break;
        default:
          LogMsg(LogLevel::kError, "Unsupported pixel format\n");
          return false;
        }
        constexpr std::array<Channel, 4> kChannels = {
          Channel::kGreen,
          Channel::kBlue,
          Channel::kRed,
          Channel::kAlpha,
        };

        const std::size_t channel_count = ChannelCount(dvf.pix_fmt);
        assert(0U < channel_count && channel_count < 4U);// NOLINT
        assert(dvf.width > 0 && dvf.height > 0);// NOLINT

        dvf.buf.count = channel_count * static_cast<std::size_t>(dvf.width * dvf.height);
        dvf.buf.data = std::make_unique<float[]>(dvf.buf.count);// NOLINT
        for (std::size_t i = 0U; i < channel_count; ++i) {
          const auto ch = channel_count > 1U ? kChannels.at(i) : Channel::kGray;
          auto pix = ChannelData(&dvf, ch);// NOLINT
          assert(!Empty(pix));// NOLINT
          assert(_av_frame->data[i] != nullptr);// NOLINT
          std::memcpy(pix.data, _av_frame->data[i], pix.count * sizeof(float));// NOLINT
        }

      } else {
        // cache the frame?
      }
#endif
    }
  } while (ret >= 0 && !got_frame);

end:
  return got_frame;
}

void DecoderImpl::_Free() noexcept
{
  if (_av_packet != nullptr) {
    av_packet_unref(_av_packet);
    av_packet_free(&_av_packet);
    assert(_av_packet == nullptr);// NOLINT
  }

  if (_av_frame != nullptr) {
    av_frame_unref(_av_frame);
    av_frame_free(&_av_frame);
    assert(_av_packet == nullptr);// NOLINT
  }

  if (_av_format_ctx != nullptr) {
    avformat_close_input(&_av_format_ctx);
    assert(_av_format_ctx == nullptr);// NOLINT
  }
}

// -----------

Decoder::Decoder(std::string path) : _pimpl{ std::make_unique<DecoderImpl>(std::move(path)) } {}
Decoder::~Decoder() = default;

auto Decoder::StreamInfo() const noexcept -> std::vector<DecodeVideoStreamInfo>
{
  return Pimpl()->StreamInfo();
}

auto Decoder::SetStreamFilterGraph(const int stream_index, std::string filter_descr) noexcept
  -> bool
{
  return Pimpl()->SetStreamFilterGraph(stream_index, std::move(filter_descr));
}

auto Decoder::DecodeVideoFrame(const int stream_index,
  const int frame_nb,
  DecodedVideoFrame &dvf) noexcept -> bool
{
  return Pimpl()->DecodeVideoFrame(stream_index, frame_nb, dvf);
}

}// namespace ilp_movie
