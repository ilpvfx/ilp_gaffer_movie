#include <algorithm>//std::fill
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

TEST_CASE("H264")
{
  std::vector<std::string> log_lines;
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.push_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  const auto init_video = [&](ilp_movie::MuxContext *mux_ctx) {
    REQUIRE(mux_ctx->impl == nullptr);
    {
      const bool ret = ilp_movie::MuxInit(mux_ctx);
      DumpLogLines(&log_lines);
      REQUIRE(ret);
    }
    REQUIRE(mux_ctx->impl != nullptr);
  };

  const auto write_video = [&](const ilp_movie::MuxContext &mux_ctx,
                             const std::uint32_t frame_count) {
    REQUIRE(frame_count > 0);

    std::vector<float> r = {};
    std::vector<float> g = {};
    std::vector<float> b = {};
    const std::size_t ch_size = static_cast<std::size_t>(mux_ctx.width * mux_ctx.height);
    r.resize(ch_size);
    g.resize(ch_size);
    b.resize(ch_size);
    bool w = true;
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
        w = false;
        break;
      }
      DumpLogLines(&log_lines);
    }
    REQUIRE(w);

    {
      const bool ret = ilp_movie::MuxFinish(mux_ctx);
      DumpLogLines(&log_lines);
      REQUIRE(ret);
    }
  };

  const auto free_video = [&](ilp_movie::MuxContext *mux_ctx) {
    REQUIRE(mux_ctx->impl != nullptr);
    ilp_movie::MuxFree(mux_ctx);
    REQUIRE(mux_ctx->impl == nullptr);
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
    mux_ctx.filename = "mux_h264_default.mov";

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("profile")
  {
    mux_ctx.filename = "mux_h264_high10.mov";
    mux_ctx.h264.profile = "high10";

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }

  SECTION("preset")
  {
    mux_ctx.filename = "mux_h264_slower.mov";
    mux_ctx.h264.preset = "slower";

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad preset")
  {
    mux_ctx.filename = "mux_h264_bad_preset.mov";
    mux_ctx.h264.preset = "__bad_preset";

    const bool ret = ilp_movie::MuxInit(&mux_ctx);
    DumpLogLines(&log_lines);
    REQUIRE(!ret);
  }

  SECTION("crf")
  {
    mux_ctx.filename = "mux_h264_crf.mov";
    mux_ctx.h264.crf = 27;

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad crf (<0)")
  {
    mux_ctx.filename = "mux_h264_bad_crf.mov";
    mux_ctx.h264.crf = -1;

    const bool r = ilp_movie::MuxInit(&mux_ctx);
    DumpLogLines(&log_lines);
    REQUIRE(!r);
  }
  SECTION("bad crf (>51)")
  {
    mux_ctx.filename = "mux_h264_bad_crf2.mov";
    mux_ctx.h264.crf = 78;

    const bool r = ilp_movie::MuxInit(&mux_ctx);
    DumpLogLines(&log_lines);
    REQUIRE(!r);
  }

  SECTION("tune")
  {
    mux_ctx.filename = "mux_h264_tune.mov";
    mux_ctx.h264.tune = "film";

    init_video(&mux_ctx);
    write_video(mux_ctx, /*frame_count=*/100);
    free_video(&mux_ctx);
  }
  SECTION("bad tune")
  {
    mux_ctx.filename = "mux_h264_bad_tune.mov";
    mux_ctx.h264.tune = "__bad_tune";

    const bool r = ilp_movie::MuxInit(&mux_ctx);
    DumpLogLines(&log_lines);
    REQUIRE(!r);
  }

  DumpLogLines(&log_lines);

  // Check that the test doesn't leak resources.
  REQUIRE(mux_ctx.impl == nullptr);
}

TEST_CASE("ProRes")
{
  std::vector<std::string> log_lines;
  ilp_movie::SetLogCallback(
    [&](int /*level*/, const char *s) { log_lines.push_back(std::string{ s }); });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);    
}

#if 0
TEST_CASE("mux init") {
  std::vector<std::string> log_lines;
  IlpGafferMovieWriter::MuxSetLogCallback(
      [&](const char *s) { log_lines.push_back(std::string{s}); });

  const auto profiles =
      IlpGafferMovieWriter::MuxVideoCodecProfiles(IlpGafferMovieWriter::MuxVideoCodec::kH264);
  for (auto &&p : profiles) {
    std::cerr << "'" << p.name << "' " << p.profile << "\n";
  }

  // Create a mux context that should be valid.
  // Verify that it is valid and then free it so that we can use the same context
  // settings again in the cases below, modifying parameters to check that certain values
  // cause failures.
  IlpGafferMovieWriter::MuxContext mux_ctx = {};
  mux_ctx.filename = "mux_init.mov";
  mux_ctx.format_name = nullptr;
  mux_ctx.width = 128;
  mux_ctx.height = 128;

  mux_ctx.codec_name = "libx264";
  //mux_ctx.codec_profile = "";
  mux_ctx.pixel_format_name = "yuv420p";

  // mux_ctx.video_codec = IlpGafferMovieWriter::MuxVideoCodec::kMPEG4;
  mux_ctx.video_codec = IlpGafferMovieWriter::MuxVideoCodec::kH264;
  // mux_ctx.video_codec = IlpGafferMovieWriter::MuxVideoCodec::kProRes;
  mux_ctx.pix_fmt = IlpGafferMovieWriter::MuxPixelFormat::kYUV420P;
  // mux_ctx.pix_fmt = IlpGafferMovieWriter::MuxPixelFormat::kGBRPF32;
  mux_ctx.frame_rate = 25;
  mux_ctx.bit_rate = 400000;
  mux_ctx.gop_size = 12;
  REQUIRE(mux_ctx.impl == nullptr);
  {
    const bool r = IlpGafferMovieWriter::MuxInit(&mux_ctx);
    DumpLogLines(log_lines);
    REQUIRE(r == true);
  }
  REQUIRE(mux_ctx.impl != nullptr);
  IlpGafferMovieWriter::MuxFree(&mux_ctx);
  REQUIRE(mux_ctx.impl == nullptr);
  log_lines.clear();

  SECTION("null context") {
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(nullptr) == false);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

  SECTION("double init") {
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == true);
    REQUIRE(mux_ctx.impl != nullptr);
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
    IlpGafferMovieWriter::MuxFree(&mux_ctx);
  }

  SECTION("bad filename") {
    mux_ctx.filename = "noextension";
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

  SECTION("bad format name") {
    mux_ctx.format_name = "not a valid format name!!";
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

  SECTION("bad filename, good format name") {
    mux_ctx.filename = nullptr;
    mux_ctx.format_name = "mpeg";
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

#if 0
  SECTION("bad width") {
    // Not divisible by 2.
    mux_ctx.width = 127;
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

  SECTION("bad height") {
    // Not divisible by 2.
    mux_ctx.height = 127;
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }
#endif

  SECTION("bad frame rate") {
    mux_ctx.frame_rate = -1;
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

#if 0
  SECTION("bad bit rate") {
    log_lines.clear();
    mux_ctx.bit_rate = -1;
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }
#endif

#if 0
  // libAV doesn't seem to consider this an error...
  SECTION("bad gop size") {
    log_lines.clear();
    mux_ctx.gop_size = -1;
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == false);
    REQUIRE(mux_ctx.impl == nullptr);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }
#endif

  // Check that test doesn't leak resources.
  REQUIRE(mux_ctx.impl == nullptr);
}
#endif

#if 0
TEST_CASE("mux write frame") {
  std::vector<std::string> log_lines;
  IlpGafferMovieWriter::MuxSetLogCallback(
      [&](const char *s) { log_lines.push_back(std::string{s}); });

  IlpGafferMovieWriter::MuxContext mux_ctx = {};
  mux_ctx.video_codec = IlpGafferMovieWriter::MuxVideoCodec::kMPEG4;
  mux_ctx.pix_fmt = IlpGafferMovieWriter::MuxPixelFormat::kYUV420P;
  mux_ctx.filename = "mux_write_frame.mov";
  mux_ctx.format_name = nullptr;
  mux_ctx.width = 128;
  mux_ctx.height = 128;
  mux_ctx.frame_rate = 25;
  mux_ctx.bit_rate = 400000;
  mux_ctx.gop_size = 12;

  SECTION("before init") {
    log_lines.clear();
    REQUIRE(mux_ctx.impl == nullptr);
    std::vector<float> r_buf(mux_ctx.width * mux_ctx.height, 0.73F);
    std::vector<float> g_buf(mux_ctx.width * mux_ctx.height, 0.15F);
    std::vector<float> b_buf(mux_ctx.width * mux_ctx.height, 0.42F);
    REQUIRE(IlpGafferMovieWriter::MuxWriteFrame(mux_ctx,
                                                IlpGafferMovieWriter::MuxFrame{
                                                    /*.width=*/mux_ctx.width,
                                                    /*.height=*/mux_ctx.height,
                                                    /*.frame_nb=*/17.F,
                                                    /*.r=*/r_buf.data(),
                                                    /*.g=*/g_buf.data(),
                                                    /*.b=*/b_buf.data(),
                                                }) ==
            IlpGafferMovieWriter::WriteStatus::kEncodingError);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
  }

  SECTION("bad frame") {
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == true);
    const int w = mux_ctx.width - 2;
    const int h = mux_ctx.width - 4;
    std::vector<float> r_buf(w * h, 0.65F);
    std::vector<float> g_buf(w * h, 0.98F);
    std::vector<float> b_buf(w * h, 0.78F);
    log_lines.clear();
    REQUIRE(IlpGafferMovieWriter::MuxWriteFrame(mux_ctx,
                                                IlpGafferMovieWriter::MuxFrame{
                                                    /*.width=*/w,
                                                    /*.height=*/h,
                                                    /*.frame_nb=*/42.F,
                                                    /*.r=*/r_buf.data(),
                                                    /*.g=*/g_buf.data(),
                                                    /*.b=*/b_buf.data(),
                                                }) ==
            IlpGafferMovieWriter::WriteStatus::kEncodingError);
    // DumpLogLines(log_lines);
    REQUIRE(!log_lines.empty());
    REQUIRE(!log_lines[0].empty());
    IlpGafferMovieWriter::MuxFree(&mux_ctx);
  }

  SECTION("good frame") {
    REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == true);
    std::vector<float> r_buf(mux_ctx.width * mux_ctx.height, 0.73F);
    std::vector<float> g_buf(mux_ctx.width * mux_ctx.height, 0.15F);
    std::vector<float> b_buf(mux_ctx.width * mux_ctx.height, 0.42F);
    REQUIRE(IlpGafferMovieWriter::MuxWriteFrame(mux_ctx,
                                                IlpGafferMovieWriter::MuxFrame{
                                                    /*.width=*/mux_ctx.width,
                                                    /*.height=*/mux_ctx.height,
                                                    /*.frame_nb=*/17.F,
                                                    /*.r=*/r_buf.data(),
                                                    /*.g=*/g_buf.data(),
                                                    /*.b=*/b_buf.data(),
                                                }) ==
            IlpGafferMovieWriter::WriteStatus::kEncodingOngoing);
    IlpGafferMovieWriter::MuxFree(&mux_ctx);
  }

  // Check that test doesn't leak resources.
  REQUIRE(mux_ctx.impl == nullptr);
}
#endif

#if 0
TEST_CASE("happy path") {
  std::vector<std::string> log_lines;
  IlpGafferMovieWriter::MuxSetLogCallback([&](const char *s) {
    log_lines.push_back(std::string{s});
    std::cout << s;
  });

  IlpGafferMovieWriter::MuxContext mux_ctx = {};
  mux_ctx.filename = "output.mov";
  mux_ctx.pix_fmt = IlpGafferMovieWriter::MuxPixelFormat::kYUV420P;
  mux_ctx.width = 128;
  mux_ctx.height = 128;
  mux_ctx.frame_rate = 25;

  REQUIRE(IlpGafferMovieWriter::MuxInit(&mux_ctx) == true);
  REQUIRE(mux_ctx.impl != nullptr);

  constexpr int kNumFrames = 100;

  std::vector<float> r;
  std::vector<float> g;
  std::vector<float> b;
  int i = 0;
  auto s = IlpGafferMovieWriter::WriteStatus::kEncodingOngoing;
  while (s == IlpGafferMovieWriter::WriteStatus::kEncodingOngoing) {
    std::cout << "Frame: " << i << "\n";
    r.clear();
    g.clear();
    b.clear();
    r.resize(mux_ctx.width * mux_ctx.height, static_cast<float>(i) / (kNumFrames - 1));
    g.resize(mux_ctx.width * mux_ctx.height, static_cast<float>(i) / (kNumFrames - 1));
    b.resize(mux_ctx.width * mux_ctx.height, static_cast<float>(i) / (kNumFrames - 1));
    if (i < 200) {
      s = IlpGafferMovieWriter::MuxWriteFrame(mux_ctx,
                                              IlpGafferMovieWriter::MuxFrame{
                                                  /*.width=*/mux_ctx.width,
                                                  /*.height=*/mux_ctx.height,
                                                  /*.frame_nb=*/static_cast<float>(i),
                                                  /*.r=*/r.data(),
                                                  /*.g=*/g.data(),
                                                  /*.b=*/b.data(),
                                              });
      REQUIRE(s == IlpGafferMovieWriter::WriteStatus::kEncodingOngoing);
    } else {
      break;
    }
    ++i;
  }
  //REQUIRE(s == IlpGafferMovieWriter::WriteStatus::kEncodingFinished);

  REQUIRE(IlpGafferMovieWriter::MuxFinish(mux_ctx));
  IlpGafferMovieWriter::MuxFreeImpl(&mux_ctx.impl);

  // Check that test doesn't leak resources.
  REQUIRE(mux_ctx.impl == nullptr);
}
#endif

}// namespace
