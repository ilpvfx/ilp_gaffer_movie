#include "ilp_movie/decoder.hpp"

#include <cassert>// assert
#include <cstring>// std::memcpy
#include <map>// std::map
#include <sstream>// std::istringstream, std::ostringstream

#include <ilp_movie/pixel_data.hpp>
#include <internal/filter_graph.hpp>
#include <internal/log_utils.hpp>

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
#include <libavformat/avformat.h>// AVStream, AVFormatContext
}
// clang-format on

namespace {

class Stream
{
public:
  Stream() = default;
  ~Stream() { _Close(); }

  // Movable.
  Stream(Stream &&rhs) noexcept = default;
  Stream &operator=(Stream &&rhs) noexcept = default;

  // Not copyable.
  Stream(const Stream &rhs) = delete;
  Stream &operator=(const Stream &rhs) = delete;

  [[nodiscard]] auto Open(AVFormatContext *const av_fmt_ctx,
    const int stream_index,
    const int thread_count = 0) noexcept -> bool
  {
    const auto exit_func = [&](const bool success) {
      if (!success) { _Close(); }
      return success;
    };

    _Close();

    if (!(av_fmt_ctx != nullptr && 0 <= stream_index
          && stream_index < static_cast<int>(av_fmt_ctx->nb_streams))) {
      log_utils_internal::LogAvError("Bad format context or stream index", AVERROR(EINVAL));
      return exit_func(/*success=*/false);
    }

    // Store pointer to stream, the format context still owns the stream.
    assert(_av_stream == nullptr);// NOLINT
    _av_stream = av_fmt_ctx->streams[stream_index];// NOLINT

    if (!(_av_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) {
      log_utils_internal::LogAvError("Only video streams supported", AVERROR(EINVAL));
      return exit_func(/*success=*/false);
    }

    const AVCodec *codec = avcodec_find_decoder(_av_stream->codecpar->codec_id);
    if (codec == nullptr) {
      log_utils_internal::LogAvError("Cannot find stream decoder", AVERROR(EINVAL));
      return exit_func(/*success=*/false);
    }

    assert(_av_codec_ctx == nullptr);// NOLINT
    _av_codec_ctx = avcodec_alloc_context3(codec);
    if (_av_codec_ctx == nullptr) {
      log_utils_internal::LogAvError("Cannot allocate codec context", AVERROR(ENOMEM));
      return exit_func(/*success=*/false);
    }

    if (thread_count > 0) {
#if 1// NOTE(tohi): experimental, not tested.
      if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {// NOLINT
        _av_codec_ctx->thread_count = thread_count;
        _av_codec_ctx->thread_type = FF_THREAD_SLICE;// NOLINT
      }
      if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {// NOLINT
        _av_codec_ctx->thread_count = thread_count;
        _av_codec_ctx->thread_type |= FF_THREAD_FRAME;// NOLINT
      }
#endif
    }

    // Copy decoder parameters to the input stream.
    if (const int ret = avcodec_parameters_to_context(_av_codec_ctx, _av_stream->codecpar);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot copy stream parameters to the decoder", ret);
      return exit_func(/*success=*/false);
    }

    // Init the video decoder.
    if (const int ret = avcodec_open2(_av_codec_ctx, codec, /*options=*/nullptr); ret < 0) {
      log_utils_internal::LogAvError("Cannot open video decoder", ret);
      return exit_func(/*success=*/false);
    }

    _av_frame = av_frame_alloc();
    if (_av_frame == nullptr) {
      log_utils_internal::LogAvError("Cannot allocate frame for stream", AVERROR(ENOMEM));
      return exit_func(/*success=*/false);
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

    return exit_func(/*success=*/true);
  }

  [[nodiscard]] auto IsOpen() const noexcept -> bool { return _av_stream != nullptr; }

  [[nodiscard]] auto Get() const noexcept -> AVStream * { return _av_stream; }

  [[nodiscard]] auto CodecContext() const noexcept -> AVCodecContext * { return _av_codec_ctx; }

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

  void FlushCodec() noexcept
  {
    if (_av_codec_ctx != nullptr) { avcodec_flush_buffers(_av_codec_ctx); }
  }

  // Send the packet to the decoder and then receive as many frames as possible.
  // For each received frame 'frame_func' will be invoked. As long as 'frame_func'
  // returns true, continue to receive frames.
  //
  // Returns true of all available frames were received without errors and 'frame_func'
  // returned true for all those frames, otherwise false.
  [[nodiscard]] auto ReceiveFrames(AVPacket *av_packet,
    const std::function<bool(AVFrame *)> &frame_func) noexcept -> bool
  {
    if (!IsOpen()) { return false; }

    int ret = avcodec_send_packet(_av_codec_ctx, av_packet);

    // Packet has been sent to the decoder. Check if it is a "flush packet".
    if (av_packet != nullptr) { av_packet_unref(av_packet); }

    if (ret < 0) {
      log_utils_internal::LogAvError("Cannot send packet to decoder", ret);
      return false;
    }

    bool keep_going = true;
    while (ret >= 0 && keep_going) {
      ret = avcodec_receive_frame(_av_codec_ctx, _av_frame);
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
  void _Close() noexcept
  {
    _av_stream = nullptr;
    if (_av_codec_ctx != nullptr) {
      avcodec_free_context(&_av_codec_ctx);
      assert(_av_codec_ctx == nullptr);// NOLINT
    }
    if (_av_frame != nullptr) {
      av_frame_free(&_av_frame);
      assert(_av_frame == nullptr);// NOLINT
    }
    _start_time = AV_NOPTS_VALUE;
    _frame_count = 0;
    _frame_rate = { /*.num=*/0, /*.den=*/1 };
  }

  AVStream *_av_stream = nullptr;
  AVCodecContext *_av_codec_ctx = nullptr;
  AVFrame *_av_frame = nullptr;

  int64_t _start_time = AV_NOPTS_VALUE;
  int64_t _frame_count = 0;
  AVRational _frame_rate = { /*.num=*/0, /*.den=*/1 };
};

}// namespace

namespace ilp_movie {

class DecoderImpl
{
public:
  [[nodiscard]] static auto SupportedFormats() -> std::vector<std::string>
  {
#if 1
    // A hard-coded list of extensions, since it turns out to be tricky
    // to get a "good" list from libav.
    static std::vector<std::string> fmt_names = { "mov", "mp4" };
#else
    // NOTE(tohi): The approach below doesn't work, it seems that most
    //             formats don't define codec_tag.
    static std::vector<std::string> fmt_names;
    if (fmt_names.empty()) {
      // Enumerate all video decoders.
      std::vector<const AVCodec *> video_decoders;
      void *codec_it = nullptr;
      const AVCodec *codec = av_codec_iterate(&codec_it);
      while (codec != nullptr) {
        const AVCodec *decoder = avcodec_find_decoder(codec->id);
        if (decoder != nullptr && decoder->type == AVMEDIA_TYPE_VIDEO) {
          video_decoders.push_back(decoder);
        }
        codec = av_codec_iterate(&codec_it);
      }

      void *demux_it = nullptr;
      const AVInputFormat *ifmt = nullptr;
      while ((ifmt = av_demuxer_iterate(&demux_it))) {// NOLINT
        if (ifmt->name != nullptr) {
          bool video_decoder_found = false;
          for (const AVCodec *video_decoder : video_decoders) {
            unsigned int tag = 0U;
            if (av_codec_get_tag2(ifmt->codec_tag, video_decoder->id, &tag) > 0) {
              video_decoder_found = true;
              break;
            }
          }

          if (video_decoder_found) {
            std::istringstream iss{ ifmt->name };
            std::string token;
            while (std::getline(iss, token, ',')) { fmt_names.push_back(token); }
          }
        }
      }
    }
#endif
    return fmt_names;
  }


  DecoderImpl() = default;
  ~DecoderImpl() { Close(); }

  DecoderImpl(DecoderImpl &&rhs) noexcept = default;
  DecoderImpl &operator=(DecoderImpl &&rhs) noexcept = default;

  // Not copyable.
  DecoderImpl(const DecoderImpl &rhs) = delete;
  DecoderImpl &operator=(const DecoderImpl &rhs) = delete;

  [[nodiscard]] auto Open(const std::string &url,
    const DecoderFilterGraphDescription &dfgd) noexcept -> bool
  {
    const auto exit_func = [&](const bool success) {
      if (!success) { Close(); }
      return success;
    };

    Close();

    _url = url;
    assert(_av_fmt_ctx == nullptr);// NOLINT
    if (const int ret =
          avformat_open_input(&_av_fmt_ctx, _url.c_str(), /*fmt=*/nullptr, /*options=*/nullptr);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot open input file for decoding", ret);
      return exit_func(/*success=*/false);
    }

    if (const int ret = avformat_find_stream_info(_av_fmt_ctx, /*options=*/nullptr); ret < 0) {
      log_utils_internal::LogAvError("Cannot find stream information for decoding", ret);
      return exit_func(/*success=*/false);
    }

    _av_fmt_ctx->flags |= AVFMT_FLAG_GENPTS;// NOLINT

    assert(_best_video_stream == -1);// NOLINT
    _best_video_stream = av_find_best_stream(_av_fmt_ctx,
      AVMEDIA_TYPE_VIDEO,
      /*wanted_stream_nb=*/-1,
      /*related_stream=*/-1,
      /*decoder_ret=*/nullptr,
      /*flags=*/0);
    if (_best_video_stream < 0) {
      log_utils_internal::LogAvError(
        "Cannot find best video stream for decoding", _best_video_stream);
      return exit_func(/*success=*/false);
    }

    // NOTE(tohi): Not using multi-threading for now.
    constexpr int kThreadCount = 0;

    // Create and open all video streams.
    for (unsigned int i = 0U; i < _av_fmt_ctx->nb_streams; ++i) {
      if (_av_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {// NOLINT
        auto video_stream = std::make_unique<Stream>();
        const int stream_index = _av_fmt_ctx->streams[i]->index;// NOLINT
        if (!video_stream->Open(_av_fmt_ctx, stream_index, kThreadCount)) {
          LogMsg(LogLevel::kError, "Failed opening video stream\n");
          return exit_func(/*success=*/false);
        }

        // Create filter graph for video stream.
        // Each video stream requires its own filter graph instance since the inputs
        // configuerd from the codec/stream parameters.
        const AVPixelFormat out_pix_fmt = av_get_pix_fmt(dfgd.out_pix_fmt_name.c_str());
        if (out_pix_fmt == AV_PIX_FMT_NONE) {
          LogMsg(LogLevel::kError, "Unrecognized filter graph output pixel format\n");
          return exit_func(/*success=*/false);
        }

        filter_graph_internal::FilterGraphDescription fg_descr{};
        fg_descr.filter_descr = dfgd.filter_descr;
        fg_descr.in.width = video_stream->CodecContext()->width;
        fg_descr.in.height = video_stream->CodecContext()->height;
        fg_descr.in.pix_fmt = video_stream->CodecContext()->pix_fmt;
        fg_descr.in.sample_aspect_ratio = video_stream->CodecContext()->sample_aspect_ratio;
        fg_descr.in.time_base = video_stream->Get()->time_base;
        fg_descr.out.pix_fmt = out_pix_fmt;
        auto fg = std::make_unique<filter_graph_internal::FilterGraph>();
        if (!fg->SetDescription(fg_descr)) {
          LogMsg(LogLevel::kError, "Failed constructing default filter graph\n");
          return exit_func(/*success=*/false);
        }

        // Populate video stream header information.
        // clang-format off
        InputVideoStreamHeader hdr{};
        hdr.stream_index = video_stream->Get()->index;
        hdr.is_best = (hdr.stream_index == _best_video_stream);
        hdr.width = video_stream->CodecContext()->width;
        hdr.height = video_stream->CodecContext()->height;
        hdr.frame_rate = { 
          /*.num=*/video_stream->FrameRate().num,
          /*.den=*/video_stream->FrameRate().den };
        hdr.pixel_aspect_ratio = { 
          /*.num=*/video_stream->CodecContext()->sample_aspect_ratio.num,
          /*.den=*/video_stream->CodecContext()->sample_aspect_ratio.den };
        hdr.first_frame_nb = 1;// Good??
        hdr.frame_count = video_stream->FrameCount();
        hdr.pix_fmt_name = 
          av_get_pix_fmt_name(video_stream->CodecContext()->pix_fmt);
        hdr.color_range_name = 
          av_color_range_name(video_stream->CodecContext()->color_range);
        hdr.color_space_name = 
          av_color_space_name(video_stream->CodecContext()->colorspace);
        hdr.color_primaries_name =
          av_color_primaries_name(video_stream->CodecContext()->color_primaries);
        // clang-format on

        _video_stream_headers.push_back(hdr);

        _video_streams[stream_index] = { std::move(video_stream), std::move(fg) };
      }
    }

    assert(_av_packet == nullptr);// NOLINT
    _av_packet = av_packet_alloc();
    if (_av_packet == nullptr) {
      log_utils_internal::LogAvError("Cannot allocate packet for decoding", AVERROR(ENOMEM));
      return exit_func(/*success=*/false);
    }

    _probe = std::invoke([&]() {
      const auto push_cb = GetLogCallback();
      const int push_log_level = GetLogLevel();
      std::ostringstream oss;
      SetLogCallback([&oss](const int /*level*/, const char *s) { oss << s; });
      SetLogLevel(LogLevel::kInfo);
      av_dump_format(_av_fmt_ctx, /*index=*/0, _url.c_str(), /*is_output=*/0);
      oss << "\nBest video stream: " << _best_video_stream << "\n";
      SetLogCallback(push_cb);
      SetLogLevel(push_log_level);
      return oss.str();
    });

    return exit_func(/*success=*/true);
  }

  [[nodiscard]] auto IsOpen() const noexcept -> bool { return !_url.empty(); }

  [[nodiscard]] auto Url() const noexcept -> const std::string & { return _url; }
  [[nodiscard]] auto Probe() const noexcept -> const std::string & { return _probe; }

  void Close()
  {
    _url.clear();
    _probe.clear();
    if (_av_fmt_ctx != nullptr) {
      avformat_close_input(&_av_fmt_ctx);
      assert(_av_fmt_ctx == nullptr);// NOLINT
    }
    if (_av_packet != nullptr) {
      av_packet_unref(_av_packet);
      av_packet_free(&_av_packet);
      assert(_av_packet == nullptr);// NOLINT
    }
    _best_video_stream = -1;
    _video_stream_headers.clear();
    _video_streams.clear();
  }

  [[nodiscard]] auto BestVideoStreamIndex() const noexcept -> int { return _best_video_stream; }

  [[nodiscard]] auto VideoStreamHeaders() const -> const std::vector<InputVideoStreamHeader> &
  {
    return _video_stream_headers;
  }

  [[nodiscard]] auto VideoStreamHeader(const int stream_index) const
    -> std::optional<InputVideoStreamHeader>
  {
    if (_video_stream_headers.empty()) { return std::nullopt; }

    const int index = stream_index == -1 ? _best_video_stream : stream_index;
    if (index < 0) { return std::nullopt; }

    // Linear search.
    for (auto &&h : _video_stream_headers) {
      if (h.stream_index == index) { return h; }
    }

    // No suitable header found.
    return std::nullopt;
  }


  [[nodiscard]] auto
    DecodeVideoFrame(int stream_index, int frame_nb, DecodedVideoFrame &dvf) noexcept -> bool
  {
    const int index = stream_index == -1 ? _best_video_stream : stream_index;

    Stream *stream = nullptr;
    filter_graph_internal::FilterGraph *filter_graph = nullptr;
    if (const auto fs_iter = _video_streams.find(index); fs_iter != _video_streams.end()) {
      stream = fs_iter->second.stream.get();
      filter_graph = fs_iter->second.filter_graph.get();
    } else {
      LogMsg(LogLevel::kWarning, "Bad stream index for decoding video frame\n");
      return false;
    }
    assert(stream != nullptr);// NOLINT
    assert(filter_graph != nullptr);// NOLINT

    // Check if frame exists in stream.
    if (!(1 <= frame_nb && frame_nb <= stream->FrameCount())) { return false; }

    const int64_t timestamp = stream->FrameToPts(frame_nb - 1);

    constexpr int kSeekFlags = AVSEEK_FLAG_BACKWARD;
    if (const int ret = av_seek_frame(_av_fmt_ctx, stream->Get()->index, timestamp, kSeekFlags);
        ret < 0) {
      log_utils_internal::LogAvError("Cannot seek to timestamp", ret);
      return false;
    }

    stream->FlushCodec();

    bool got_frame = false;
    bool keep_going = true;
    int ret = 0;
    while (ret >= 0 && keep_going) {
      // Read a packet, which for video streams corresponds to one frame.
      ret = av_read_frame(_av_fmt_ctx, _av_packet);

      // A packet was successfully read, check if was from the stream we are interested in.
      if (ret >= 0 && _av_packet->stream_index != stream->Get()->index) {
        // Ignore packets that are not from the video stream we are trying to read from.
        av_packet_unref(_av_packet);
        continue;
      }

      // TODO(tohi): Can we seek based on packet PTS? Not all formats/streams support
      //             packet PTS.

      keep_going =
        stream->ReceiveFrames(ret >= 0 ? _av_packet : /*flush*/ nullptr, [&](AVFrame *dec_frame) {
          // TODO(tohi): Can we seek based on frame PTS? Check frame PTS before sending to filter
          // graph?
          bool keep_going_dec = true;
          if (dec_frame->pts <= timestamp
              && timestamp < (dec_frame->pts + dec_frame->pkt_duration)) {
            keep_going_dec = filter_graph->FilterFrames(dec_frame, [&](AVFrame *filt_frame) {
              // Check if the frame has a PTS/duration that matches our seek target.
              bool keep_going_filt = true;
              if (filt_frame->pts <= timestamp
                  && timestamp < (filt_frame->pts + filt_frame->pkt_duration)) {
                // Found a frame with a good PTS so we do not need to look for more frames.
                // This is our one chance.
                keep_going_filt = false;

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

                // clang-format off
                dvf.width = filt_frame->width;
                dvf.height = filt_frame->height;
                dvf.key_frame = filt_frame->key_frame > 0;
                dvf.frame_nb = frame_nb;
                dvf.pixel_aspect_ratio = { 
                  /*.num=*/filt_frame->sample_aspect_ratio.num,
                  /*.den=*/filt_frame->sample_aspect_ratio.den 
                };
                // clang-format on

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
          }
          return keep_going_dec;
        });
    }
    return got_frame;

    // _av_frame->crop_top
    // _av_frame->crop_bottom
    // _av_frame->crop_left
    // _av_frame->crop_right
    // av_frame_apply_cropping(...)
  }

private:
  std::string _url;
  std::string _probe;

  AVFormatContext *_av_fmt_ctx = nullptr;
  AVPacket *_av_packet = nullptr;

  int _best_video_stream = -1;
  std::vector<InputVideoStreamHeader> _video_stream_headers;

  struct FilteredStream
  {
    std::unique_ptr<Stream> stream;
    std::unique_ptr<filter_graph_internal::FilterGraph> filter_graph;
  };

  std::map<int, FilteredStream> _video_streams;
};// namespace ilp_movie

// -----------

auto Decoder::SupportedFormats() -> std::vector<std::string>
{
  return DecoderImpl::SupportedFormats();
}

Decoder::Decoder() : _pimpl{ std::make_unique<DecoderImpl>() } {}
Decoder::~Decoder() = default;

auto Decoder::Open(const std::string &url, const DecoderFilterGraphDescription &dfgd) noexcept
  -> bool
{
  return Pimpl()->Open(url, dfgd);
}

auto Decoder::IsOpen() const noexcept -> bool { return Pimpl()->IsOpen(); }

auto Decoder::Url() const noexcept -> const std::string & { return Pimpl()->Url(); }
auto Decoder::Probe() const noexcept -> const std::string & { return Pimpl()->Probe(); }

void Decoder::Close() noexcept { Pimpl()->Close(); }

auto Decoder::BestVideoStreamIndex() const noexcept -> int
{
  return Pimpl()->BestVideoStreamIndex();
}

auto Decoder::VideoStreamHeaders() const -> const std::vector<InputVideoStreamHeader> &
{
  return Pimpl()->VideoStreamHeaders();
}

auto Decoder::VideoStreamHeader(const int stream_index) const
  -> std::optional<InputVideoStreamHeader>
{
  return Pimpl()->VideoStreamHeader(stream_index);
}

auto Decoder::DecodeVideoFrame(const int stream_index,
  const int frame_nb,
  DecodedVideoFrame &dvf) noexcept -> bool
{
  return Pimpl()->DecodeVideoFrame(stream_index, frame_nb, dvf);
}

}// namespace ilp_movie
