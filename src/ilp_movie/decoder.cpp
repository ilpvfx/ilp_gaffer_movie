#include "ilp_movie/decoder.hpp"

#include <cassert>// assert
#include <cstdint>// int64_t
#include <cstring>// std::memcpy
#include <map>// std::map

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
  explicit Stream(AVFormatContext *const av_fmt_ctx,
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
        if (stream->codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {// NOLINT
          _av_codec_context->thread_count = thread_count;
          _av_codec_context->thread_type = FF_THREAD_SLICE;// NOLINT
        }
        if (stream->codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {// NOLINT
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

      _av_frame = av_frame_alloc();
      if (_av_frame == nullptr) {
        log_utils_internal::LogAvError("Cannot allocate frame", AVERROR(ENOMEM));
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

  void Flush() noexcept { avcodec_flush_buffers(_av_codec_context); }

  [[nodiscard]] auto ReceiveFrames(AVPacket *av_packet,
    const std::function<bool(AVFrame *)> &frame_func) noexcept -> bool
  {
    int ret = avcodec_send_packet(_av_codec_context, av_packet);

    // Packet has been sent to the decoder.
    av_packet_unref(av_packet);

    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot send packet to decoder", ret);
      return false;
    }

    bool keep_going = true;
    while (ret >= 0 && keep_going) {
      ret = avcodec_receive_frame(_av_codec_context, _av_frame);
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {// NOLINT
        // Not really an error, just time to quit.
        ret = 0;
        break;
      }
      if (ret < 0) {
        // Legitimate decoding error.
        log_utils_internal::LogAvError("Cannot decode frame", ret);
        av_frame_unref(_av_frame);
        break;
      }

      _av_frame->pts = _av_frame->best_effort_timestamp;
      keep_going = frame_func(_av_frame);
      av_frame_unref(_av_frame);
    }
    return ret >= 0 && keep_going;
  }

private:
  void _Free() noexcept
  {
    if (_av_codec_context != nullptr) { avcodec_free_context(&_av_codec_context); }
    if (_av_frame != nullptr) { av_frame_free(&_av_frame); }
  }

  AVStream *_av_stream = nullptr;
  AVCodecContext *_av_codec_context = nullptr;
  AVFrame *_av_frame = nullptr;

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

  int _best_video_stream = -1;

  struct FilteredStream
  {
    std::unique_ptr<Stream> stream;
    std::unique_ptr<filter_graph_internal::FilterGraph> filter_graph;
  };

  std::map<int, FilteredStream> _streams;
  // std::map<int, std::unique_ptr<filter_graph_internal::FilterGraph>> _filter_graphs;
  //  std::vector<DecodeVideoStreamInfo> _stream_info;
  //   int _decode_stream = -1;
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
        auto stream = std::make_unique<Stream>(_av_format_ctx, si, kThreadCount);
        filter_graph_internal::FilterGraphArgs fg_args{};
        fg_args.filter_descr = "null";
        fg_args.sws_flags = "";
        fg_args.in.width = stream->CodecContext()->width;
        fg_args.in.height = stream->CodecContext()->height;
        fg_args.in.pix_fmt = stream->CodecContext()->pix_fmt;
        fg_args.in.sample_aspect_ratio = stream->CodecContext()->sample_aspect_ratio;
        fg_args.in.time_base = stream->Get()->time_base;
        fg_args.out.pix_fmt = AV_PIX_FMT_GBRPF32;
        auto filter_graph = std::make_unique<filter_graph_internal::FilterGraph>(fg_args);
        _streams[si->index] = { std::move(stream), std::move(filter_graph) };
      } catch (std::invalid_argument &e) {
        // todo!!!
        // re-throw!?
      }
    }
  }
}

DecoderImpl::~DecoderImpl() { _Free(); }

auto DecoderImpl::StreamInfo() const noexcept -> std::vector<DecodeVideoStreamInfo>
{
  std::vector<DecodeVideoStreamInfo> r;
  for (auto &&[index, fs] : _streams) {
    assert(index == fs.stream->Index());// NOLINT
    DecodeVideoStreamInfo dvsi{};
    dvsi.stream_index = fs.stream->Index();
    dvsi.is_best = (dvsi.stream_index == _best_video_stream);
    dvsi.width = fs.stream->CodecContext()->width;
    dvsi.height = fs.stream->CodecContext()->height;
    dvsi.frame_rate = { /*.num=*/fs.stream->FrameRate().num, /*.den=*/fs.stream->FrameRate().den };
    dvsi.first_frame_nb = 1;// Good??
    dvsi.frame_count = fs.stream->FrameCount();
    dvsi.pix_fmt_name = av_get_pix_fmt_name(fs.stream->CodecContext()->pix_fmt);
    dvsi.color_range_name = av_color_range_name(fs.stream->CodecContext()->color_range);
    dvsi.color_space_name = av_color_space_name(fs.stream->CodecContext()->colorspace);
    dvsi.color_primaries_name = av_color_primaries_name(fs.stream->CodecContext()->color_primaries);
    r.push_back(dvsi);
  }
  return r;
}

auto DecoderImpl::SetStreamFilterGraph(const int stream_index, std::string filter_descr) noexcept
  -> bool
{
  const auto fs_iter = _streams.find(stream_index);
  if (fs_iter == _streams.end()) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Bad stream index for filter graph\n");
    return false;
  }
  Stream *stream = fs_iter->second.stream.get();

  filter_graph_internal::FilterGraphArgs fg_args{};
  fg_args.filter_descr = std::move(filter_descr);
  fg_args.sws_flags = "";
  fg_args.in.width = stream->CodecContext()->width;
  fg_args.in.height = stream->CodecContext()->height;
  fg_args.in.pix_fmt = stream->CodecContext()->pix_fmt;
  fg_args.in.sample_aspect_ratio = stream->CodecContext()->sample_aspect_ratio;
  fg_args.in.time_base = stream->Get()->time_base;
  fg_args.out.pix_fmt = AV_PIX_FMT_GBRPF32;// TODO(tohi): is this always good?

  try {
    fs_iter->second.filter_graph = std::make_unique<filter_graph_internal::FilterGraph>(fg_args);
  } catch (std::exception &) {
    ilp_movie::LogMsg(ilp_movie::LogLevel::kWarning, "Failed constructing filter graph\n");
    return false;
  }

  return true;
}

auto DecoderImpl::DecodeVideoFrame(int stream_index, int frame_nb, DecodedVideoFrame &dvf) noexcept
  -> bool
{
  Stream *stream = nullptr;
  filter_graph_internal::FilterGraph *filter_graph = nullptr;
  if (const auto fs_iter = _streams.find(stream_index); fs_iter != _streams.end()) {
    stream = fs_iter->second.stream.get();
    filter_graph = fs_iter->second.filter_graph.get();
  } else {
    // TODO: log warning
    return false;
  }
  assert(stream != nullptr);// NOLINT
  assert(filter_graph != nullptr);// NOLINT

  const int64_t timestamp = stream->FrameToPts(frame_nb - 1);

  constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
  if (const int ret = av_seek_frame(_av_format_ctx, stream->Index(), timestamp, kSeekFlags);
      ret < 0) {
    log_utils_internal::LogAvError("Cannot seek to timestamp", ret);
    return false;
  }

  stream->Flush();

  bool got_frame = false;// TODO??!?!?
  bool keep_going = true;
  int ret = 0;
  while (ret >= 0 && keep_going) {
    // Read a packet, which for video streams corresponds to one frame.
    ret = av_read_frame(_av_format_ctx, _av_packet);

    // A packet was successfully read, check if was from the stream we are interested in.
    if (ret >= 0 && _av_packet->stream_index != stream->Index()) {
      // Ignore packets that are not from the video stream we are trying to read from.
      av_packet_unref(_av_packet);
      continue;
    }

    // TODO(tohi): Can we seek based on packet PTS?

    keep_going =
      stream->ReceiveFrames(ret >= 0 ? _av_packet : /*flush*/ nullptr, [&](AVFrame *frame) {
        // TODO(tohi): Can we seek based on frame PTS? Check frame PTS before sending to filter
        // graph?

        return filter_graph->FilterFrames(frame, [&](AVFrame *filt_frame) {
          // Check if the frame has a PTS/duration that matches our seek target.
          bool keep_going_filt = true;
          if (filt_frame->pts <= timestamp
              && timestamp < (filt_frame->pts + filt_frame->pkt_duration)) {
            // Found a frame with a good PTS so we do not need to look for more frames.
            // This is our one chance.
            keep_going_filt = false;

            dvf.width = filt_frame->width;
            dvf.height = filt_frame->height;
            dvf.key_frame = filt_frame->key_frame > 0;
            dvf.frame_nb = frame_nb;
            dvf.pixel_aspect_ratio =
              (filt_frame->sample_aspect_ratio.num == 0 && filt_frame->sample_aspect_ratio.den == 1)
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
              return keep_going_filt;
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
            return keep_going_filt;
          }
          return keep_going_filt;
        });
      });
  }

  return got_frame;

  // _av_frame->decode_error_flags

  // _av_frame->crop_top
  // _av_frame->crop_bottom
  // _av_frame->crop_left
  // _av_frame->crop_right
  // av_frame_apply_cropping(...)
}

void DecoderImpl::_Free() noexcept
{
  if (_av_packet != nullptr) {
    av_packet_unref(_av_packet);
    av_packet_free(&_av_packet);
  }

  if (_av_format_ctx != nullptr) { avformat_close_input(&_av_format_ctx); }
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
