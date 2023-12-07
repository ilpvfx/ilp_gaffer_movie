#include <algorithm>// std::fill
#include <iostream>// std::cout, std::cerr
#include <string>// std::string
#include <string_view>// std::string_view
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>

using namespace std::literals; // "hello"sv

namespace {

TEST_CASE("H264")
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

  SECTION("default")
  {
    // Create a mux context with as many default settings as possible (i.e. minimal config) that
    // should be valid.
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_default.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      ilp_movie::H264::EncodeParameters{});
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(mux_params->filename == "/tmp/test_data/mux_h264_default.mov"sv);
    REQUIRE(mux_params->width == 164);
    REQUIRE(mux_params->height == 128);
    REQUIRE(mux_params->frame_rate == 24.0);
    REQUIRE(!mux_params->pix_fmt.empty());
    REQUIRE(!mux_params->color_range.empty());
    REQUIRE(!mux_params->color_primaries.empty());
    REQUIRE(!mux_params->colorspace.empty());
    REQUIRE(!mux_params->color_trc.empty());
    REQUIRE(!mux_params->codec_name.empty());
    REQUIRE(!mux_params->preset.empty());
    REQUIRE(!mux_params->tune.empty());
    REQUIRE(mux_params->crf >= 0);
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("preset")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.preset = ilp_movie::H264::Preset::kMedium;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_medium.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad preset")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.preset = "__bad_preset"sv;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_bad_preset.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }

  SECTION("pixel format")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.pix_fmt = ilp_movie::H264::PixelFormat::kYuv422p10;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_yuv422p10.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad pixel format")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.pix_fmt = "__bad_pix_fmt"sv;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_bad_pix_fmt.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }

  SECTION("tune")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.tune = ilp_movie::H264::Tune::kFilm;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_tune.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad tune")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.pix_fmt = "__bad_tune"sv;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_bad_tune.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }

  SECTION("crf")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.crf = 27;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_crf.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }
  SECTION("bad crf (<0)")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.crf = -1;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_neg_crf.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }
  SECTION("bad crf (>51)")
  {
    ilp_movie::H264::EncodeParameters enc_params = {};
    enc_params.crf = 78;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_h264_big_crf.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }
}

TEST_CASE("ProRes")
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

  SECTION("default")
  {
    // Create a mux context with as many default settings as possible (i.e. minimal config) that
    // should be valid.
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_default.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      ilp_movie::ProRes::EncodeParameters{});
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(mux_params->filename == "/tmp/test_data/mux_prores_default.mov"sv);
    REQUIRE(mux_params->width == 164);
    REQUIRE(mux_params->height == 128);
    REQUIRE(mux_params->frame_rate == 24.0);
    REQUIRE(!mux_params->pix_fmt.empty());
    REQUIRE(!mux_params->codec_name.empty());
    REQUIRE(mux_params->profile >= 0);
    REQUIRE(mux_params->qscale >= 0);
    REQUIRE(!mux_params->vendor.empty());
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("proxy")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::kProxy;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_proxy.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("lt")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::kLt;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_lt.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("standard")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::kStandard;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_standard.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("hq")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::kHq;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_hq.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);
  }

  SECTION("4444")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::k4444;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_4444.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);

    // TODO(tohi): Test that we get a different pix_fmt if we request alpha?
  }

  SECTION("4444xq")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::k4444xq;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_4444xq.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    write_video(*mux_ctx, /*frame_count=*/100);

    // TODO(tohi): Test that we get a different pix_fmt if we request alpha?
  }

  SECTION("bad alpha")
  {
    // Only some profiles (4444) support alpha.
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.profile_name = ilp_movie::ProRes::ProfileName::kHq;
    enc_params.alpha = true;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_bad_alpha.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(!mux_params.has_value());
  }

  SECTION("bad qscale")
  {
    ilp_movie::ProRes::EncodeParameters enc_params = {};
    enc_params.qscale = -13;
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_bad_qscale.mov",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      enc_params);
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }

  SECTION("mp4")
  {
    // mp4 container currently doesn't support ProRes codec.
    const auto mux_params = ilp_movie::MakeMuxParameters(
      /*filename=*/"/tmp/test_data/mux_prores_bad_format.mp4",
      /*width=*/164,
      /*height=*/128,
      /*frame_rate=*/24.0,
      ilp_movie::ProRes::EncodeParameters{});
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    REQUIRE(dump_log_on_fail(ilp_movie::MakeMuxContext(*mux_params) == nullptr));
  }
}

}// namespace
