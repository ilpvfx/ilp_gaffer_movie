#include <chrono>
#include <iostream>// std::cout, std::cerr
#include <string>// std::string
#include <thread>
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

// Helper function - has no effect on test results.
static void DumpLogLines(std::vector<std::string> *log_lines)
{
  for (auto &&line : *log_lines) { std::cerr << line; }
  log_lines->clear();
}

static void DumpLogLinesIf(std::vector<std::string> *log_lines, const bool cond)
{
  if (cond) { DumpLogLines(log_lines); }
}

namespace {

TEST_CASE("test")
{
  std::vector<std::string> log_lines;
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.emplace_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

#if 1
  const auto init_mux = [&](ilp_movie::MuxContext *mux_ctx) {
    REQUIRE(mux_ctx->impl == nullptr);
    {
      const bool ret = ilp_movie::MuxInit(mux_ctx);
      DumpLogLinesIf(&log_lines, !ret);
      REQUIRE(ret);
    }
    REQUIRE(mux_ctx->impl != nullptr);
  };
  const auto write_frames = [&](const ilp_movie::MuxContext &mux_ctx,
                              const std::uint32_t frame_count) {
    REQUIRE(frame_count > 0);

    const auto w = static_cast<std::size_t>(mux_ctx.width);
    const auto h = static_cast<std::size_t>(mux_ctx.height);
    std::vector<float> r = {};
    std::vector<float> g = {};
    std::vector<float> b = {};
    r.resize(w * h);
    g.resize(w * h);
    b.resize(w * h);
    bool write_err = false;
    for (std::uint32_t i = 0; i < frame_count; ++i) {
      const float pixel_value = static_cast<float>(i) / static_cast<float>(frame_count - 1U);
      std::fill(std::begin(r), std::end(r), pixel_value);
      std::fill(std::begin(g), std::end(g), pixel_value);
      std::fill(std::begin(b), std::end(b), pixel_value);
      if (!ilp_movie::MuxWriteFrame(mux_ctx,
            ilp_movie::MuxFrame{
              /*.width=*/mux_ctx.width,
              /*.height=*/mux_ctx.height,
              /*.frame_nb=*/static_cast<float>(i),
              /*.r=*/r.data(),
              /*.g=*/g.data(),
              /*.b=*/b.data(),
            })) {
        write_err = true;
        break;
      }
      DumpLogLines(&log_lines);
    }
    REQUIRE(!write_err);

    {
      const bool ret = ilp_movie::MuxFinish(mux_ctx);
      DumpLogLines(&log_lines);
      REQUIRE(ret);
    }
  };
  const auto free_mux = [&](ilp_movie::MuxContext *mux_ctx) {
    REQUIRE(mux_ctx->impl != nullptr);
    ilp_movie::MuxFree(mux_ctx);
    REQUIRE(mux_ctx->impl == nullptr);
  };
#endif

  const auto init_demux = [&](ilp_movie::DemuxContext *demux_ctx) {
    REQUIRE(demux_ctx->impl == nullptr);
    {
      //ilp_movie::DemuxFrame first_frame = {};
      const bool ret = ilp_movie::DemuxInit(demux_ctx/*, &first_frame*/);
      DumpLogLinesIf(&log_lines, !ret);
      REQUIRE(ret);
    }
    REQUIRE(demux_ctx->impl != nullptr);
  };
  const auto free_demux = [&](ilp_movie::DemuxContext *demux_ctx) {
    REQUIRE(demux_ctx->impl != nullptr);
    ilp_movie::DemuxFree(demux_ctx);
    REQUIRE(demux_ctx->impl == nullptr);
  };

#if 1
  ilp_movie::MuxContext mux_ctx = {};
  mux_ctx.filename = "/tmp/demux.mp4";
  mux_ctx.codec_name = "libx264";
  // mux_ctx.color_range = "tv";
  mux_ctx.width = 128;
  mux_ctx.height = 128;
  init_mux(&mux_ctx);
  write_frames(mux_ctx, /*frame_count=*/100);
  free_mux(&mux_ctx);

  std::this_thread::sleep_for(std::chrono::seconds(2));
#endif

  ilp_movie::DemuxContext demux_ctx = {};
  demux_ctx.filename = "/tmp/demux.mp4";
  init_demux(&demux_ctx);
  // read_frames()
  free_demux(&demux_ctx);

  DumpLogLines(&log_lines);
}

}// namespace