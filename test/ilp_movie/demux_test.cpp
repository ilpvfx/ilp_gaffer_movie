#include <iostream>// std::cout, std::cerr
#include <string>// std::string
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/demux.hpp>
#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>

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

  const auto init_demux = [&](ilp_movie::DemuxContext *demux_ctx) {
    REQUIRE(demux_ctx->impl == nullptr);
    {
      const bool ret = ilp_movie::DemuxInit(demux_ctx);
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


  ilp_movie::MuxContext mux_ctx = {};
  mux_ctx.filename = "demux.mov";
  mux_ctx.codec_name = "libx264";
  mux_ctx.width = 128;
  mux_ctx.height = 128;
  init_mux(&mux_ctx);
  write_frames(mux_ctx, /*frame_count=*/100);
  free_mux(&mux_ctx);


  ilp_movie::DemuxContext demux_ctx = {};
  demux_ctx.filename = "demux.mov";
  init_demux(&demux_ctx);
  //read_frames()
  free_demux(&demux_ctx);

  DumpLogLines(&log_lines);
}

}// namespace