#include <algorithm>// std::shuffle
#include <iostream>// std::cout, std::cerr
#include <random>// std::default_random_engine
#include <sstream>// std::ostringstream
#include <string>// std::string
#include <vector>// std::vector

// clang-format off
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
// clang-format on

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/demux.hpp>
#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>
#include <ilp_movie/pixel_data.hpp>

using namespace std::literals;// "hello"sv

#if 0
namespace fas {

struct SeekEntry
{
  int display_index;
  int64_t first_packet_dts;
  int64_t last_packet_dts;
};

struct SeekTable
{
  SeekEntry *array;// TODO - std::vector
  bool completed;
  int num_frames;// total number of frames
  int num_entries;// ie, number of seek-points (keyframes)
  int allocated_size;
} seek_table;

SeekTable InitSeekTable(int initial_size)
{
  SeekTable table = {};

  if (initial_size < 0) { initial_size = 100; }

  table.num_entries = 0;
  table.num_frames = -1;
  table.completed = false;

  table.array = new SeekEntry[initial_size];// NOLINT

  if (nullptr == table.array) {
    table.allocated_size = 0;
  } else {
    table.allocated_size = initial_size;
  }

  return table;
}

void ReleaseSeekTable(SeekTable *table)
{
  table->num_entries = 0;
  table->num_frames = -1;
  table->completed = false;

  if (nullptr == table || nullptr == table->array) { return; }

  delete[] table->array;// NOLINT
}


struct SeekContext
{
  bool is_video_active;
  bool is_frame_available;

  int current_frame_index;

  SeekTable seek_table;

  /* ffmpeg */
  AVFormatContext *format_context;
  AVCodecContext *codec_context;
  int stream_idx;

  AVFrame *frame_buffer;// internal buffer

  AVFrame *rgb_frame_buffer;// extra AVFrames (for color conversion)
  uint8_t *rgb_buffer;// actual data buffer for rgb_frame_buffer (needs to be freed seperately)
  bool rgb_already_converted;// color_convert(frame_buffer) == rgb_frame_buffer (so we
                             // don't need to repeat conversions)

  AVFrame *gray8_frame_buffer;
  uint8_t *gray8_buffer;
  bool gray8_already_converted;

  int64_t current_dts;// decoding timestamp of the most recently parsed packet
  int64_t
    previous_dts;// for previous packet (always use previous packet for seek_table (workaround))
  int64_t keyframe_packet_dts;// dts of most recent keyframe packet
  int64_t first_dts;// for very first packet (needed in seek, for first keyframe)
};

int CloseVideo(SeekContext *ctx)
{
  if (nullptr == ctx) {
    // return private_show_error ("NULL context provided for fas_close_video()",
    // FAS_INVALID_ARGUMENT);
    return -1;
  }

  if (!(ctx->is_video_active)) {
    // private_show_warning("Redundant attempt to close an inactive video");
    return 0;
  }

  if (ctx->codec_context != nullptr) {
    if (avcodec_find_decoder(ctx->codec_context->codec_id) != nullptr) {
      avcodec_close(ctx->codec_context);
    }
  }

  if (ctx->format_context != nullptr) { avformat_close_input(&(ctx->format_context)); }
  if (ctx->rgb_frame_buffer != nullptr) { av_free(ctx->rgb_frame_buffer); }
  if (ctx->gray8_frame_buffer != nullptr) { av_free(ctx->gray8_frame_buffer); }
  if (ctx->rgb_buffer != nullptr) { av_free(ctx->rgb_buffer); }
  if (ctx->gray8_buffer != nullptr) { av_free(ctx->gray8_buffer); }
  if (ctx->frame_buffer != nullptr) { av_free(ctx->frame_buffer); }
  ReleaseSeekTable(&(ctx->seek_table));

  ctx->is_video_active = false;

  delete ctx;// NOLINT

  return 0;
}

int StepForward(SeekContext *ctx)
{
  if ((nullptr == ctx) || (!ctx->is_video_active)) {
    return -1;
    // return private_show_error ("invalid or unopened context", FAS_INVALID_ARGUMENT);
  }

  if (!ctx->is_frame_available) {
    // private_show_warning("tried to advance after end of frames");
    // return FAS_SUCCESS;
    return -1;
  }

  ctx->current_frame_index++;

  AVPacket packet;
  for (;;) {
    if (av_read_frame(ctx->format_context, &packet) < 0) {
      /* finished */
      ctx->is_frame_available = false;
      ctx->seek_table.completed = true;
      return 0;// FAS_SUCCESS
    }

    int frameFinished;
    if (packet.stream_index == ctx->stream_idx) {
      ctx->previous_dts = context->current_dts;
      ctx->current_dts = packet.dts;

      /* seek support: set first_dts */
      if (ctx->first_dts == AV_NOPTS_VALUE) { ctx->first_dts = packet.dts; }

      /* seek support: set key-packet info to previous packet's dts, when possible */
      /* note this -1 approach to setting the packet is a workaround for a common failure. setting
         to 0 would work just incur a huge penalty in videos that needed -1. Might be worth testing.
      */
      if (packet.flags & AV_PKT_FLAG_KEY) {// NOLINT
        // fprintf(stderr, "Packet: (F:%d %lld %lld)\n", context->current_frame_index, packet.pts,
        // packet.dts);

        if (ctx->previous_dts == AV_NOPTS_VALUE) {
          ctx->keyframe_packet_dts = packet.dts;
        } else {
          ctx->keyframe_packet_dts = ctx->previous_dts;
        }
      }

      avcodec_decode_video2(
        ctx->codec_context, ctx->frame_buffer, &frameFinished, packet.data, packet.size);

      if (frameFinished) {
        /* seek support: (try to) add entry to seek_table */
        if (context->frame_buffer->key_frame) {
          //		fprintf(stderr, "Frame : (PXX F%d: %lld %lld)\n",
          // context->current_frame_index, packet.pts, packet.dts);

          SeekEntry entry = {};
          entry.display_index = ctx->current_frame_index;
          entry.first_packet_dts = ctx->keyframe_packet_dts;
          entry.last_packet_dts = packet.dts;

          if (fas_get_frame_index(context) == FIRST_FRAME_INDEX)
            entry.first_packet_dts = context->first_dts;

          seek_append_table_entry(&context->seek_table, entry);
        }

        if (context->current_frame_index - FIRST_FRAME_INDEX + 1 > context->seek_table.num_frames)
          context->seek_table.num_frames = context->current_frame_index - FIRST_FRAME_INDEX + 1;

        break;
      }
    }

    av_free_packet(&packet);
  }

  context->rgb_already_converted = FAS_FALSE;
  context->gray8_already_converted = FAS_FALSE;
  av_free_packet(&packet);
  return FAS_SUCCESS;
}

static int OpenVideo(SeekContext **ctx, const char *filename)
{
  // if (ctx == nullptr) { return error; }

  *ctx = nullptr;

  auto *local_ctx = new SeekContext;// NOLINT
  std::memset(local_ctx, 0, sizeof(SeekContext));
  local_ctx->is_video_active = true;
  local_ctx->is_frame_available = true;
  local_ctx->current_frame_index = -1;
  local_ctx->current_dts = AV_NOPTS_VALUE;
  local_ctx->previous_dts = AV_NOPTS_VALUE;
  local_ctx->keyframe_packet_dts = AV_NOPTS_VALUE;
  local_ctx->first_dts = AV_NOPTS_VALUE;

  local_ctx->seek_table = InitSeekTable(-1); /* default starting size */

  if (avformat_open_input(&(local_ctx->format_context), filename, nullptr, nullptr) != 0) {
    CloseVideo(local_ctx);
    // return private_show_error("failure to open file", FAS_UNSUPPORTED_FORMAT);
    return -1;
  }

  if (av_find_stream_info(fas_context->format_context) < 0) {
    CloseVideo(local_ctx);
    return -1;
    // fas_close_video(fas_context);
    // return private_show_error ("could not extract stream information", FAS_UNSUPPORTED_FORMAT);
  }

  // av_dump_format(fas_context->format_context, 0, file_path, 0);

#if 0
  int stream_idx;
  for (stream_idx = 0; stream_idx < fas_context->format_context->nb_streams; stream_idx++) {
    if (fas_context->format_context->streams[stream_idx]->codec->codec_type == CODEC_TYPE_VIDEO) {
      fas_context->stream_idx = stream_idx;
      fas_context->codec_context = fas_context->format_context->streams[stream_idx]->codec;
      break;
    }
  }
#endif

  // Select the video stream.
  const AVCodec *dec = nullptr;
  local_ctx->stream_idx = av_find_best_stream(local_ctx->format_context,
    AVMEDIA_TYPE_VIDEO,
    /*wanted_stream_nb=*/-1,
    /*related_stream=*/-1,
    &dec,
    /*flags=*/0);
  if (local_ctx->stream_idx < 0) {
    // ilp_movie::LogAvError("Cannot find a video stream in the input file", *video_stream_index);
    // return false;
    CloseVideo(local_ctx);
    return -1;
  }

  local_ctx->codec_context = avcodec_alloc_context3(dec);
  if (*dec_ctx == nullptr) {
    // ilp_movie::LogAvError("Cannot allocate context", AVERROR(ENOMEM));
    CloseVideo(local_ctx);
    return -1;
  }

  // Copy decoder parameters to the input stream.
  if (const int ret = avcodec_parameters_to_context(local_ctx->codec_context,
        local_ctx->format_context->streams[local_ctx->stream_idx]->codecpar);// NOLINT
      ret < 0) {
    // ilp_movie::LogAvError("Could not copy decoder parameters to the output stream", ret);
    CloseVideo(local_ctx);
    return false;
  }

  // Init the video decoder.
  if (const int ret = avcodec_open2(*dec_ctx, dec, /*options=*/nullptr); ret < 0) {
    // ilp_movie::LogAvError("Cannot open video decoder", ret);
    CloseVideo(local_ctx);
    return false;
  }

  local_ctx->frame_buffer = avcodec_alloc_frame();
  if (local_ctx->frame_buffer == nullptr) {
    CloseVideo(local_ctx);
    return -1;
    // fas_close_video(fas_context);
    // return private_show_error("failed to allocate frame buffer", FAS_OUT_OF_MEMORY);
  }

  fas_context->rgb_frame_buffer = avcodec_alloc_frame();
  if (fas_context->rgb_frame_buffer == nullptr) {
    CloseVideo(local_ctx);
    return -1;
    // fas_close_video(fas_context);
    // return private_show_error("failed to allocate rgb frame buffer", FAS_OUT_OF_MEMORY);
  }

  fas_context->gray8_frame_buffer = avcodec_alloc_frame();
  if (fas_context->gray8_frame_buffer == NULL) {
    // fas_close_video(fas_context);
    // return private_show_error("failed to allocate gray8 frame buffer", FAS_OUT_OF_MEMORY);
    CloseVideo(local_ctx);
    return -1;
  }

  local_ctx->rgb_buffer = nullptr;
  local_ctx->gray8_buffer = nullptr;
  local_ctx->rgb_already_converted = false;
  local_ctx->gray8_already_converted = false;

  *ctx = local_ctx;


  if (FAS_SUCCESS != fas_step_forward(*context_ptr))
    return private_show_error("failure decoding first frame", FAS_NO_MORE_FRAMES);

  if (!fas_frame_available(*context_ptr))
    return private_show_error(
      "couldn't find a first frame (no valid frames in video stream)", FAS_NO_MORE_FRAMES);


  return FAS_SUCCESS;
}

}// namespace fas
#endif

// NOTE(tohi): The mux frame channel pointers should be initialized so that they refer to the demux
//             frame buffer.
struct FramePair
{
  ilp_movie::MuxFrame mux_frame = {};
  ilp_movie::DemuxFrame demux_frame = {};
};

[[nodiscard]] static auto MakeFramePair(const int w,
  const int h,
  const int frame_nb,
  const ilp_movie::PixelFormat pix_fmt,
  FramePair *const frames) -> bool
{
  if (!(w > 0 && h > 0)) { return false; }

  frames->demux_frame.width = w;
  frames->demux_frame.height = h;
  frames->demux_frame.frame_nb = frame_nb;
  frames->demux_frame.pixel_aspect_ratio = 1.0;
  frames->demux_frame.pix_fmt = pix_fmt;
  frames->demux_frame.buf.count = ilp_movie::ChannelCount(pix_fmt)
                                  * static_cast<std::size_t>(frames->demux_frame.width)
                                  * static_cast<std::size_t>(frames->demux_frame.height);
  frames->demux_frame.buf.data = std::make_unique<float[]>(frames->demux_frame.buf.count);// NOLINT

  const auto r = ilp_movie::ChannelData(&(frames->demux_frame), ilp_movie::Channel::kRed);
  const auto g = ilp_movie::ChannelData(&(frames->demux_frame), ilp_movie::Channel::kGreen);
  const auto b = ilp_movie::ChannelData(&(frames->demux_frame), ilp_movie::Channel::kBlue);
  const auto a = ilp_movie::ChannelData(&(frames->demux_frame), ilp_movie::Channel::kAlpha);

  // TMP!!
  const float cx =
    0.5F * static_cast<float>(frames->demux_frame.width) + static_cast<float>(frame_nb);
  const float cy = 0.5F * static_cast<float>(frames->demux_frame.height);
  const float rad = 0.25F
                    * static_cast<float>(
                      std::min(frames->demux_frame.width, frames->demux_frame.height));// in pixels
  const float norm =
    1.0F
    / (0.5F
       * static_cast<float>(std::sqrt(frames->demux_frame.width * frames->demux_frame.width
                                      + frames->demux_frame.height * frames->demux_frame.height)));

  for (int y = 0; y < frames->demux_frame.height; ++y) {
    for (int x = 0; x < frames->demux_frame.width; ++x) {
      const auto i = static_cast<size_t>(y) * static_cast<size_t>(frames->demux_frame.width)
                     + static_cast<size_t>(x);
      const float dx = static_cast<float>(x) - cx;
      const float dy = static_cast<float>(y) - cy;
      const float d = std::sqrt(dx * dx + dy * dy) - rad;

      switch (frames->demux_frame.pix_fmt) {
      case ilp_movie::PixelFormat::kGray:
        r.data[i] = std::abs(d) * norm;// NOLINT
        break;
      case ilp_movie::PixelFormat::kRGB:
        if (d < 0) {
          b.data[i] = std::abs(d) * norm;// NOLINT
        } else {
          r.data[i] = d * norm;// NOLINT
        }
        g.data[i] = std::clamp(static_cast<float>(frame_nb) / 400, 0.F, 1.F);// NOLINT
        break;
      case ilp_movie::PixelFormat::kRGBA:
        if (d < 0) {
          r.data[i] = std::abs(d) * norm;// NOLINT
        } else {
          g.data[i] = d * norm;// NOLINT
        }
        b.data[i] = std::clamp(static_cast<float>(frame_nb) / 400, 0.F, 1.F);// NOLINT
        a.data[i] = (d < 0) ? 1.F : 0.F;// NOLINT
        break;
      case ilp_movie::PixelFormat::kNone:
      default:
        return false;
      }
    }
  }

  frames->mux_frame.width = frames->demux_frame.width;
  frames->mux_frame.height = frames->demux_frame.height;
  frames->mux_frame.frame_nb = static_cast<float>(frames->demux_frame.frame_nb);
  frames->mux_frame.r = ilp_movie::ChannelData(frames->demux_frame.width,
    frames->demux_frame.height,
    ilp_movie::Channel::kRed,
    frames->demux_frame.buf.data.get(),
    frames->demux_frame.buf.count)
                          .data;
  frames->mux_frame.g = ilp_movie::ChannelData(frames->demux_frame.width,
    frames->demux_frame.height,
    ilp_movie::Channel::kGreen,
    frames->demux_frame.buf.data.get(),
    frames->demux_frame.buf.count)
                          .data;
  frames->mux_frame.b = ilp_movie::ChannelData(frames->demux_frame.width,
    frames->demux_frame.height,
    ilp_movie::Channel::kBlue,
    frames->demux_frame.buf.data.get(),
    frames->demux_frame.buf.count)
                          .data;
  frames->mux_frame.a = ilp_movie::ChannelData(frames->demux_frame.width,
    frames->demux_frame.height,
    ilp_movie::Channel::kAlpha,
    frames->demux_frame.buf.data.get(),
    frames->demux_frame.buf.count)
                          .data;

  return true;
}

namespace {

TEST_CASE("seek - prores")
{
  std::vector<std::string> log_lines = {};
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.emplace_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  const auto dump_log_on_fail = [&](const bool cond) {
    if (!cond) {
      // Dump log before exiting.
      for (auto &&line : log_lines) { std::cerr << line; }
      log_lines.clear();
    }
    return cond;
  };

  const auto write_frames = [&](const ilp_movie::MuxContext &mux_ctx, const int frame_count) {
    REQUIRE(dump_log_on_fail(frame_count > 0));

    for (int i = 0; i < frame_count; ++i) {
      FramePair f = {};
      REQUIRE(dump_log_on_fail(MakeFramePair(
        mux_ctx.params.width, mux_ctx.params.height, i, ilp_movie::PixelFormat::kRGB, &f)));
      REQUIRE(dump_log_on_fail(ilp_movie::MuxWriteFrame(mux_ctx, f.mux_frame)));
    }
    REQUIRE(dump_log_on_fail(ilp_movie::MuxFinish(mux_ctx)));
  };

  const auto seek_frames = [&](const ilp_movie::DemuxContext &demux_ctx, const int frame_count) {
    REQUIRE(frame_count > 0);

#if 0
    std::vector<int> frame_range;
    frame_range.push_back(80);
#else
    std::vector<int> frame_range(static_cast<std::size_t>(frame_count));
    std::iota(std::begin(frame_range), std::end(frame_range), 1);
    auto rng = std::default_random_engine{ /*seed=*/1981 };// NOLINT
    std::shuffle(std::begin(frame_range), std::end(frame_range), rng);
#endif

    for (const auto frame_nb : frame_range) {
      ilp_movie::DemuxFrame seek_frame = {};
      REQUIRE(dump_log_on_fail(ilp_movie::DemuxSeek(demux_ctx, frame_nb, &seek_frame)));
      REQUIRE(seek_frame.frame_nb == frame_nb);
      REQUIRE(seek_frame.key_frame);// ProRes is all intraframe, so all key frames!

      FramePair f = {};
      REQUIRE(MakeFramePair(
        seek_frame.width, seek_frame.height, frame_nb, ilp_movie::PixelFormat::kRGB, &f));

      const auto r = ilp_movie::ChannelData(seek_frame.width,
        seek_frame.height,
        ilp_movie::Channel::kRed,
        seek_frame.buf.data.get(),
        seek_frame.buf.count);
      const auto g = ilp_movie::ChannelData(seek_frame.width,
        seek_frame.height,
        ilp_movie::Channel::kGreen,
        seek_frame.buf.data.get(),
        seek_frame.buf.count);
      const auto b = ilp_movie::ChannelData(seek_frame.width,
        seek_frame.height,
        ilp_movie::Channel::kBlue,
        seek_frame.buf.data.get(),
        seek_frame.buf.count);
      double r_max_err = std::numeric_limits<double>::lowest();
      double g_max_err = std::numeric_limits<double>::lowest();
      double b_max_err = std::numeric_limits<double>::lowest();
      double r_avg_err = 0.0;
      double g_avg_err = 0.0;
      double b_avg_err = 0.0;
      for (std::size_t i = 0; i < r.count; ++i) {
        const auto err = static_cast<double>(std::abs(r.data[i] - f.mux_frame.r[i]));// NOLINT
        r_avg_err += err;
        r_max_err = std::max(err, r_max_err);
      }
      r_avg_err /= static_cast<double>(r.count);
      for (std::size_t i = 0; i < g.count; ++i) {
        const auto err = static_cast<double>(std::abs(g.data[i] - f.mux_frame.g[i]));// NOLINT
        g_avg_err += err;
        g_max_err = std::max(err, g_max_err);
      }
      g_avg_err /= static_cast<double>(g.count);
      for (std::size_t i = 0; i < b.count; ++i) {
        const auto err = static_cast<double>(std::abs(b.data[i] - f.mux_frame.b[i]));// NOLINT
        b_avg_err += err;
        b_max_err = std::max(err, b_max_err);
      }
      b_avg_err /= static_cast<double>(b.count);

      {
        std::ostringstream oss;
        oss << "Frame " << frame_nb << "\n"
            << "r | avg: " << r_avg_err << ", max: " << r_max_err << "\n"
            << "g | avg: " << g_avg_err << ", max: " << g_max_err << "\n"
            << "b | avg: " << b_avg_err << ", max: " << b_max_err << "\n";
        ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
      }

      REQUIRE(g_avg_err < 0.01);// NOLINT
      REQUIRE(g_avg_err < 0.01);// NOLINT
      REQUIRE(b_avg_err < 0.01);// NOLINT
      REQUIRE(r_max_err < 0.02);// NOLINT
      REQUIRE(g_max_err < 0.02);// NOLINT
      REQUIRE(b_max_err < 0.02);// NOLINT
    }
  };

  SECTION("RGB")
  {
    // From https://academysoftwarefoundation.github.io/EncodingGuidelines/Encoding.html:
    //
    // ffmpeg
    //  -r 24
    //  -start_number 1
    //  -i inputfile.%04d.png
    //  -vf "scale=in_color_matrix=bt709:out_color_matrix=bt709"
    //  -vframes 100
    //  -c:v prores_ks
    //  -profile:v 3
    //  -pix_fmt yuv422p10le
    //  -color_range tv
    //  -colorspace bt709
    //  -color_primaries bt709
    //  -color_trc iec61966-2-1
    //  outputfile.mov

    constexpr std::string_view kFilename = "/tmp/test_data/demux_test.mov"sv;
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr int kFrameCount = 200;

    // Write/mux.
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::kHq;// -profile:v 3
    auto mux_params = ilp_movie::MakeMuxParameters(kFilename,
      kWidth,
      kHeight,
      /*frame_rate=*/24.0,// -r 24
      enc_params);
    REQUIRE(mux_params.has_value());
    REQUIRE(mux_params->pix_fmt == "yuv422p10le"sv);// -pix_fmt yuv422p10le
    mux_params->color_range = ilp_movie::ColorRange::kTv;// -color_range tv
    mux_params->colorspace = ilp_movie::Colorspace::kBt709;// -colorspace bt709
    mux_params->color_primaries = ilp_movie::Colorspace::kBt709;// -color_primaries bt709
    mux_params->color_trc = ilp_movie::ColorTrc::kIec61966_2_1;// -color_trc iec61966-2-1
    // -vf "scale=in_color_matrix=bt709:out_color_matrix=bt709"
    mux_params->filter_graph = "scale=in_color_matrix=bt709:out_color_matrix=bt709"sv;
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_frames(*mux_ctx, kFrameCount);
    mux_ctx.reset();

    // Read/demux
    ilp_movie::DemuxParams demux_params = {};
    demux_params.filename = kFilename;
    demux_params.filter_graph =
      //"scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709";
      "scale=in_color_matrix=bt709:out_color_matrix=bt709";
    auto demux_ctx = ilp_movie::DemuxMakeContext(demux_params);
    REQUIRE(dump_log_on_fail(demux_ctx != nullptr));

    REQUIRE(demux_ctx->info.width == kWidth);
    REQUIRE(demux_ctx->info.height == kHeight);
    REQUIRE(demux_ctx->info.first_frame_nb == 1);
    REQUIRE(demux_ctx->info.first_frame_nb + demux_ctx->info.frame_count - 1 == 200);

    seek_frames(*demux_ctx, 3 /*kFrameCount*/);

    demux_ctx.reset();
  }

  dump_log_on_fail(false);// TMP!!
}

// TEST_CASE("seek - h.264")
[[maybe_unused]] void foo()
{
  std::vector<std::string> log_lines = {};
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.emplace_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);
  const auto dump_log_on_fail = [&](const bool cond) {
    if (!cond) {
      // Dump log before exiting.
      for (auto &&line : log_lines) { std::cerr << line; }
      log_lines.clear();
    }
    return cond;
  };

  const auto write_frames = [&](const ilp_movie::MuxContext &mux_ctx, const int frame_count) {
    REQUIRE(dump_log_on_fail(frame_count > 0));

    for (int i = 0; i < frame_count; ++i) {
      FramePair f = {};
      REQUIRE(dump_log_on_fail(MakeFramePair(
        mux_ctx.params.width, mux_ctx.params.height, i, ilp_movie::PixelFormat::kRGB, &f)));
      REQUIRE(dump_log_on_fail(ilp_movie::MuxWriteFrame(mux_ctx, f.mux_frame)));
    }
    REQUIRE(dump_log_on_fail(ilp_movie::MuxFinish(mux_ctx)));
  };

  const auto seek_frames = [&](const ilp_movie::DemuxContext &demux_ctx, const int frame_count) {
    REQUIRE(frame_count > 0);

#if 0
    std::vector<int> frame_range;
    frame_range.push_back(80);
#else
    std::vector<int> frame_range(static_cast<std::size_t>(frame_count - 1));
    std::iota(std::begin(frame_range), std::end(frame_range), 1);
    auto rng = std::default_random_engine{ /*seed=*/1981 };// NOLINT
    std::shuffle(std::begin(frame_range), std::end(frame_range), rng);
#endif

    for (const auto frame_nb : frame_range) {
      ilp_movie::DemuxFrame seek_frame = {};
      REQUIRE(dump_log_on_fail(ilp_movie::DemuxSeek(demux_ctx, frame_nb, &seek_frame)));
      REQUIRE(seek_frame.frame_nb == frame_nb);

      FramePair f = {};
      REQUIRE(MakeFramePair(
        seek_frame.width, seek_frame.height, frame_nb, ilp_movie::PixelFormat::kRGB, &f));

      const auto r = ilp_movie::ChannelData(seek_frame.width,
        seek_frame.height,
        ilp_movie::Channel::kRed,
        seek_frame.buf.data.get(),
        seek_frame.buf.count);
      const auto g = ilp_movie::ChannelData(seek_frame.width,
        seek_frame.height,
        ilp_movie::Channel::kGreen,
        seek_frame.buf.data.get(),
        seek_frame.buf.count);
      const auto b = ilp_movie::ChannelData(seek_frame.width,
        seek_frame.height,
        ilp_movie::Channel::kBlue,
        seek_frame.buf.data.get(),
        seek_frame.buf.count);
      float r_max_err = std::numeric_limits<float>::lowest();
      float g_max_err = std::numeric_limits<float>::lowest();
      float b_max_err = std::numeric_limits<float>::lowest();
      float r_avg_err = 0.F;
      float g_avg_err = 0.F;
      float b_avg_err = 0.F;
      for (std::size_t i = 0; i < r.count; ++i) {
        const float err = std::abs(r.data[i] - f.mux_frame.r[i]);// NOLINT
        r_avg_err += err;
        r_max_err = std::max(err, r_max_err);
      }
      r_avg_err /= static_cast<float>(r.count);
      for (std::size_t i = 0; i < g.count; ++i) {
        const float err = std::abs(g.data[i] - f.mux_frame.g[i]);// NOLINT
        g_avg_err += err;
        g_max_err = std::max(err, g_max_err);
      }
      g_avg_err /= static_cast<float>(g.count);
      for (std::size_t i = 0; i < b.count; ++i) {
        const float err = std::abs(b.data[i] - f.mux_frame.b[i]);// NOLINT
        b_avg_err += err;
        b_max_err = std::max(err, b_max_err);
      }
      b_avg_err /= static_cast<float>(b.count);

      {
        std::ostringstream oss;
        oss << "Frame " << frame_nb << "\n"
            << "r | avg: " << r_avg_err << ", max: " << r_max_err << "\n"
            << "g | avg: " << g_avg_err << ", max: " << g_max_err << "\n"
            << "b | avg: " << b_avg_err << ", max: " << b_max_err << "\n";
        ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
      }

      REQUIRE(0.F <= r_avg_err);// NOLINT
      REQUIRE(0.F <= g_avg_err);// NOLINT
      REQUIRE(0.F <= b_avg_err);// NOLINT
      REQUIRE(r_max_err < 0.25F);// NOLINT
      REQUIRE(g_max_err < 0.25F);// NOLINT
      REQUIRE(b_max_err < 0.25F);// NOLINT
    }
  };


  SECTION("RGB")
  {
    const char *const kFilename = "/tmp/test_data/demux_test_h264.mp4";
    constexpr int kFrameCount = 200;

    // Write/mux


    ilp_movie::MuxParameters mux_params = {};

    // asdasd
    mux_params.filename = kFilename;
    mux_params.codec_name = "libx264";
    // mux_params.
    //  mux_ctx.color_range = "tv";
    mux_params.width = 512;
    mux_params.height = 512;
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_frames(*mux_ctx, kFrameCount);
    mux_ctx.reset();

    // Read/demux
    ilp_movie::DemuxParams demux_params = {};
    demux_params.filename = kFilename;
    auto demux_ctx = ilp_movie::DemuxMakeContext(demux_params);
    REQUIRE(dump_log_on_fail(demux_ctx != nullptr));

    REQUIRE(demux_ctx->info.width == 512);
    REQUIRE(demux_ctx->info.height == 512);
    REQUIRE(demux_ctx->info.first_frame_nb == 1);
    REQUIRE(demux_ctx->info.first_frame_nb + demux_ctx->info.frame_count - 1 == 200);

    seek_frames(*demux_ctx, 3 /* kFrameCount*/);

    demux_ctx.reset();
  }


  dump_log_on_fail(false);// TMP!!
}

}// namespace