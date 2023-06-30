#include <algorithm>// std::fill
#include <iostream>// std::cout, std::cerr
#include <string>// std::string
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>

namespace {

TEST_CASE("h264")
{
  std::vector<std::string> log_lines;
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

  const auto write_video = [&](const ilp_movie::MuxContext &mux_ctx,
                             const std::uint32_t frame_count) {
    REQUIRE(frame_count > 0);

    const auto w = static_cast<std::size_t>(mux_ctx.params.width);
    const auto h = static_cast<std::size_t>(mux_ctx.params.height);
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
      REQUIRE(dump_log_on_fail(ilp_movie::MuxWriteFrame(mux_ctx,
        ilp_movie::MuxFrame{
          /*.width=*/mux_ctx.params.width,
          /*.height=*/mux_ctx.params.height,
          /*.frame_nb=*/static_cast<float>(i),
          /*.r=*/r.data(),
          /*.g=*/g.data(),
          /*.b=*/b.data(),
        })));
    }
    REQUIRE(dump_log_on_fail(ilp_movie::MuxFinish(mux_ctx)));
  };

  // Create a mux context with as many default settings as possible (i.e. minimal config) that
  // should be valid.
  ilp_movie::MuxParameters mux_params = {};
  mux_params.codec_name = "libx264";
  mux_params.width = 128;
  mux_params.height = 128;

  SECTION("default")
  {
    // Check that we have sane default values!
    mux_params.filename = "/tmp/mux_h264_default.mov";
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("preset")
  {
    mux_params.filename = "/tmp/mux_h264_medium.mov";
    mux_params.h264.cfg.preset = ilp_movie::h264::Preset::kMedium;
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad preset")
  {
    mux_params.filename = "/tmp/mux_h264_bad_preset.mov";
    mux_params.h264.cfg.preset = "__bad_preset";
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }

  SECTION("pixel format")
  {
    mux_params.filename = "/tmp/mux_h264_yuv422p10.mov";
    mux_params.h264.cfg.pix_fmt = ilp_movie::h264::PixelFormat::kYuv422p10;
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad pixel format")
  {
    mux_params.filename = "/tmp/mux_h264_bad_pix_fmt.mov";
    mux_params.h264.cfg.pix_fmt = "__bad_pix_fmt";
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }

  SECTION("tune")
  {
    mux_params.filename = "/tmp/mux_h264_tune.mov";
    mux_params.h264.cfg.tune = ilp_movie::h264::Tune::kFilm;
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad tune")
  {
    mux_params.filename = "/tmp/mux_h264_bad_tune.mov";
    mux_params.h264.cfg.tune = "__bad_tune";
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }

  SECTION("crf")
  {
    mux_params.filename = "/tmp/mux_h264_crf.mov";
    mux_params.h264.cfg.crf = 27;
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad crf (<0)")
  {
    mux_params.filename = "/tmp/mux_h264_bad_crf.mov";
    mux_params.h264.cfg.crf = -1;
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }
  SECTION("bad crf (>51)")
  {
    mux_params.filename = "/tmp/mux_h264_bad_crf2.mov";
    mux_params.h264.cfg.crf = 78;
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }
}

TEST_CASE("ProRes")
{
  std::vector<std::string> log_lines;
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

  const auto write_video = [&](const ilp_movie::MuxContext &mux_ctx,
                             const std::uint32_t frame_count) {
    REQUIRE(frame_count > 0);

    const auto w = static_cast<std::size_t>(mux_ctx.params.width);
    const auto h = static_cast<std::size_t>(mux_ctx.params.height);
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
      REQUIRE(dump_log_on_fail(ilp_movie::MuxWriteFrame(mux_ctx,
        ilp_movie::MuxFrame{
          /*.width=*/mux_ctx.params.width,
          /*.height=*/mux_ctx.params.height,
          /*.frame_nb=*/static_cast<float>(i),
          /*.r=*/r.data(),
          /*.g=*/g.data(),
          /*.b=*/b.data(),
        })));
    }
    REQUIRE(dump_log_on_fail(ilp_movie::MuxFinish(mux_ctx)));
  };

  // Create a mux context with as many default settings as possible (i.e. minimal config) that
  // should be valid.
  ilp_movie::MuxParameters mux_params = {};
  mux_params.codec_name = "prores_ks";
  mux_params.width = 128;
  mux_params.height = 128;

  SECTION("default")
  {
    // Check that we have sane default values!
    mux_params.filename = "/tmp/mux_prores_default.mov";
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("proxy")
  {
    mux_params.filename = "/tmp/mux_prores_proxy.mov";
    mux_params.pro_res.cfg = ilp_movie::ProRes::Config::Preset("proxy").value();
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("lt")
  {
    mux_params.filename = "/tmp/mux_prores_proxy.mov";
    mux_params.pro_res.cfg = ilp_movie::ProRes::Config::Preset("lt").value();
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("standard")
  {
    mux_params.filename = "/tmp/mux_prores_standard.mov";
    mux_params.pro_res.cfg = ilp_movie::ProRes::Config::Preset("standard").value();
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("hq")
  {
    mux_params.filename = "/tmp/mux_prores_hq.mov";
    mux_params.pro_res.cfg = ilp_movie::ProRes::Config::Preset("hq").value();
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("4444")
  {
    mux_params.filename = "/tmp/mux_prores_4444.mov";
    mux_params.pro_res.cfg = ilp_movie::ProRes::Config::Preset("4444").value();
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("4444xq")
  {
    mux_params.filename = "/tmp/mux_prores_4444xq.mov";
    mux_params.pro_res.cfg = ilp_movie::ProRes::Config::Preset("4444xq").value();
    auto mux_ctx = ilp_movie::MakeMuxContext(mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("bad qscale")
  {
    mux_params.filename = "/tmp/mux_prores_bad_qscale.mov";
    mux_params.pro_res.cfg.qscale = -13;
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }

  SECTION("mp4")
  {
    // mp4 container currently doesn't support ProRes encoder.
    mux_params.filename = "/tmp/mux_prores.mp4";
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(mux_params) == nullptr));
  }
}

}// namespace
