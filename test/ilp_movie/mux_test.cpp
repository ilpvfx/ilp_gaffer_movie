#include <algorithm>// std::fill
#include <iostream>// std::cout, std::cerr
#include <string>// std::string
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>

// Helper function - has no effect on test results.
static void DumpLogLines(std::vector<std::string> *log_lines)
{
  for (auto &&line : *log_lines) { std::cerr << line; }
  log_lines->clear();
}

namespace {

TEST_CASE("h264")
{
  std::vector<std::string> log_lines;
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.emplace_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  const auto require = [&](const bool cond) {
    if (!cond) {
      // Dump log before exiting.
      DumpLogLines(&log_lines);
    }
    REQUIRE(cond);
  };

  const auto init_video = [&](ilp_movie::MuxContext *mux_ctx) {
    require(mux_ctx->impl == nullptr);
    require(ilp_movie::MuxInit(mux_ctx));
    require(mux_ctx->impl != nullptr);
  };

  const auto write_video = [&](const ilp_movie::MuxContext &mux_ctx,
                             const std::uint32_t frame_count) {
    require(frame_count > 0);

    const auto w = static_cast<std::size_t>(mux_ctx.width);
    const auto h = static_cast<std::size_t>(mux_ctx.height);
    std::vector<float> r = {};
    std::vector<float> g = {};
    std::vector<float> b = {};
    r.resize(w * h);
    g.resize(w * h);
    b.resize(w * h);
    for (std::uint32_t i = 0; i < frame_count; ++i) {
      const float pixel_value = static_cast<float>(i) / static_cast<float>(frame_count - 1U);
      std::fill(std::begin(r), std::end(r), pixel_value);
      std::fill(std::begin(g), std::end(g), pixel_value);
      std::fill(std::begin(b), std::end(b), pixel_value);
      require(ilp_movie::MuxWriteFrame(mux_ctx,
            ilp_movie::MuxFrame{
              /*.width=*/mux_ctx.width,
              /*.height=*/mux_ctx.height,
              /*.frame_nb=*/static_cast<float>(i),
              /*.r=*/r.data(),
              /*.g=*/g.data(),
              /*.b=*/b.data(),
            }));
    }
    require(ilp_movie::MuxFinish(mux_ctx));
  };

  const auto free_video = [&](ilp_movie::MuxContext *mux_ctx) {
    require(mux_ctx->impl != nullptr);
    ilp_movie::MuxFree(mux_ctx);
    require(mux_ctx->impl == nullptr);
  };

  // Create a mux context with as many default settings as possible (i.e. minimal config) that
  // should be valid.
  ilp_movie::MuxContext mux_ctx = {};
  mux_ctx.codec_name = "libx264";
  mux_ctx.width = 128;
  mux_ctx.height = 128;

  SECTION("default")
  {
    // Check that we have sane default values!
    mux_ctx.filename = "/tmp/mux_h264_default.mov";
    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("preset")
  {
    mux_ctx.filename = "/tmp/mux_h264_medium.mov";
    mux_ctx.h264.cfg.preset = ilp_movie::h264::Preset::kMedium;
    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad preset")
  {
    mux_ctx.filename = "/tmp/mux_h264_bad_preset.mov";
    mux_ctx.h264.cfg.preset = "__bad_preset";
    require(!ilp_movie::MuxInit(&mux_ctx));
  }

  SECTION("pixel format")
  {
    mux_ctx.filename = "/tmp/mux_h264_yuv422p10.mov";
    mux_ctx.h264.cfg.pix_fmt = ilp_movie::h264::PixelFormat::kYuv422p10;
    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad pixel format")
  {
    mux_ctx.filename = "/tmp/mux_h264_bad_pix_fmt.mov";
    mux_ctx.h264.cfg.pix_fmt = "__bad_pix_fmt";
    require(!ilp_movie::MuxInit(&mux_ctx));
  }

  SECTION("tune")
  {
    mux_ctx.filename = "/tmp/mux_h264_tune.mov";
    mux_ctx.h264.cfg.tune = ilp_movie::h264::Tune::kFilm;
    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad tune")
  {
    mux_ctx.filename = "/tmp/mux_h264_bad_tune.mov";
    mux_ctx.h264.cfg.tune = "__bad_tune";
    require(!ilp_movie::MuxInit(&mux_ctx));
  }

  SECTION("crf")
  {
    mux_ctx.filename = "/tmp/mux_h264_crf.mov";
    mux_ctx.h264.cfg.crf = 27;
    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad crf (<0)")
  {
    mux_ctx.filename = "/tmp/mux_h264_bad_crf.mov";
    mux_ctx.h264.cfg.crf = -1;
    require(!ilp_movie::MuxInit(&mux_ctx));
  }
  SECTION("bad crf (>51)")
  {
    mux_ctx.filename = "/tmp/mux_h264_bad_crf2.mov";
    mux_ctx.h264.cfg.crf = 78;
    require(!ilp_movie::MuxInit(&mux_ctx));
  }

  // Check that the test doesn't leak resources.
  require(mux_ctx.impl == nullptr);
}

TEST_CASE("ProRes")
{
  std::vector<std::string> log_lines;
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.emplace_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  const auto require = [&](const bool cond) {
    if (!cond) {
      // Dump log before exiting.
      DumpLogLines(&log_lines);
    }
    REQUIRE(cond);
  };

  const auto init_video = [&](ilp_movie::MuxContext *mux_ctx) {
    require(mux_ctx->impl == nullptr);
    require(ilp_movie::MuxInit(mux_ctx));
    require(mux_ctx->impl != nullptr);
  };

  const auto write_video = [&](const ilp_movie::MuxContext &mux_ctx,
                             const std::uint32_t frame_count) {
    require(frame_count > 0);

    const auto w = static_cast<std::size_t>(mux_ctx.width);
    const auto h = static_cast<std::size_t>(mux_ctx.height);
    std::vector<float> r = {};
    std::vector<float> g = {};
    std::vector<float> b = {};
    r.resize(w * h);
    g.resize(w * h);
    b.resize(w * h);
    for (std::uint32_t i = 0; i < frame_count; ++i) {
      const float pixel_value = static_cast<float>(i) / static_cast<float>(frame_count - 1U);
      std::fill(std::begin(r), std::end(r), pixel_value);
      std::fill(std::begin(g), std::end(g), pixel_value);
      std::fill(std::begin(b), std::end(b), pixel_value);
      require(ilp_movie::MuxWriteFrame(mux_ctx,
            ilp_movie::MuxFrame{
              /*.width=*/mux_ctx.width,
              /*.height=*/mux_ctx.height,
              /*.frame_nb=*/static_cast<float>(i),
              /*.r=*/r.data(),
              /*.g=*/g.data(),
              /*.b=*/b.data(),
            }));
    }
    require(ilp_movie::MuxFinish(mux_ctx));
  };

  const auto free_video = [&](ilp_movie::MuxContext *mux_ctx) {
    require(mux_ctx->impl != nullptr);
    ilp_movie::MuxFree(mux_ctx);
    require(mux_ctx->impl == nullptr);
  };

  // Create a mux context with as many default settings as possible (i.e. minimal config) that
  // should be valid.
  ilp_movie::MuxContext mux_ctx = {};
  mux_ctx.codec_name = "prores_ks";
  mux_ctx.width = 128;
  mux_ctx.height = 128;

  SECTION("default")
  {
    // Check that we have sane default values!
    mux_ctx.filename = "/tmp/mux_prores_default.mov";

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("proxy") {
    mux_ctx.filename = "/tmp/mux_prores_proxy.mov";
    mux_ctx.pro_res.cfg = ilp_movie::ProRes::Config::Preset("proxy").value();

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("lt") {
    mux_ctx.filename = "/tmp/mux_prores_proxy.mov";
    mux_ctx.pro_res.cfg = ilp_movie::ProRes::Config::Preset("lt").value();

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("standard") {
    mux_ctx.filename = "/tmp/mux_prores_standard.mov";
    mux_ctx.pro_res.cfg = ilp_movie::ProRes::Config::Preset("standard").value();

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("hq") {
    mux_ctx.filename = "/tmp/mux_prores_hq.mov";
    mux_ctx.pro_res.cfg = ilp_movie::ProRes::Config::Preset("hq").value();

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("4444") {
    mux_ctx.filename = "/tmp/mux_prores_4444.mov";
    mux_ctx.pro_res.cfg = ilp_movie::ProRes::Config::Preset("4444").value();

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("4444xq") {
    mux_ctx.filename = "/tmp/mux_prores_4444xq.mov";
    mux_ctx.pro_res.cfg = ilp_movie::ProRes::Config::Preset("4444xq").value();

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("bad qscale") {
    mux_ctx.filename = "/tmp/mux_prores_bad_qscale.mov";
    mux_ctx.pro_res.cfg.qscale = -13;
    require(!ilp_movie::MuxInit(&mux_ctx));
  }

  SECTION("mp4") {
    // mp4 container currently doesn't support ProRes encoder.
    mux_ctx.filename = "/tmp/mux_prores.mp4";
    require(!ilp_movie::MuxInit(&mux_ctx));
  }

  // Check that the test doesn't leak resources.
  require(mux_ctx.impl == nullptr);
}

}// namespace
