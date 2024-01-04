#include <algorithm>// std::shuffle
#include <iostream>// std::cout, std::cerr
#include <mutex>//std::call_once
#include <random>// std::default_random_engine
#include <sstream>// std::ostringstream
#include <string>// std::string
#include <thread>//std::thread
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include "ilp_movie/decoder.hpp"
#include "ilp_movie/frame.hpp"
#include "ilp_movie/log.hpp"
#include "ilp_movie/mux.hpp"

using namespace std::literals;// "hello"sv

namespace {

// NOTE(tohi): The mux frame channel pointers should be initialized so that they refer to the demux
//             frame buffer.
struct FramePair
{
  ilp_movie::FrameView mux_frame = {};
  ilp_movie::Frame demux_frame = {};
};

[[nodiscard]] auto MakeFramePair(const int w,
  const int h,
  const int frame_nb,
  const char *pix_fmt_name,
  FramePair *const fp) -> bool
{
  if (!(w > 0 && h > 0)) { return false; }

  fp->demux_frame.hdr.width = w;
  fp->demux_frame.hdr.height = h;
  fp->demux_frame.hdr.pixel_aspect_ratio = { /*.num=*/1, /*.den=*/1 };
  fp->demux_frame.hdr.frame_nb = frame_nb;
  fp->demux_frame.hdr.pix_fmt_name = pix_fmt_name;
  fp->demux_frame.buf = std::make_unique<uint8_t[]>(// NOLINT
    ilp_movie::GetBufferSize(
      fp->demux_frame.hdr.pix_fmt_name, fp->demux_frame.hdr.width, fp->demux_frame.hdr.height)
      .value());
  if (!ilp_movie::FillArrays(/*out*/ fp->demux_frame.data,
        /*out*/ fp->demux_frame.linesize,
        fp->demux_frame.buf.get(),
        fp->demux_frame.hdr.pix_fmt_name,
        fp->demux_frame.hdr.width,
        fp->demux_frame.hdr.height)) {
    return false;
  }

  using ilp_movie::CompPixelData;
  namespace Comp = ilp_movie::Comp;
  const auto r = CompPixelData<float>(fp->demux_frame.data, Comp::kR, w, h, pix_fmt_name);
  const auto g = CompPixelData<float>(fp->demux_frame.data, Comp::kG, w, h, pix_fmt_name);
  const auto b = CompPixelData<float>(fp->demux_frame.data, Comp::kB, w, h, pix_fmt_name);
  const auto a = CompPixelData<float>(fp->demux_frame.data, Comp::kA, w, h, pix_fmt_name);

  // TMP!!
  const float cx = 0.5F * static_cast<float>(fp->demux_frame.hdr.width)
                   + static_cast<float>(fp->demux_frame.hdr.frame_nb);
  const float cy = 0.5F * static_cast<float>(fp->demux_frame.hdr.height);
  const float rad = 0.25F
                    * static_cast<float>(
                      std::min(fp->demux_frame.hdr.width, fp->demux_frame.hdr.height));// in pixels
  const float norm =
    1.0F
    / (0.5F
       * static_cast<float>(std::sqrt(fp->demux_frame.hdr.width * fp->demux_frame.hdr.width
                                      + fp->demux_frame.hdr.height * fp->demux_frame.hdr.height)));

  for (int y = 0; y < fp->demux_frame.hdr.height; ++y) {
    for (int x = 0; x < fp->demux_frame.hdr.width; ++x) {
      const auto i = static_cast<size_t>(y) * static_cast<size_t>(fp->demux_frame.hdr.width)
                     + static_cast<size_t>(x);
      const float dx = static_cast<float>(x) - cx;
      const float dy = static_cast<float>(y) - cy;
      const float d = std::sqrt(dx * dx + dy * dy) - rad;

      // TODO(tohi): Support grayscale?!
      size_t p = 0U;
      if (!Empty(b) && !Empty(r)) {
        if (d < 0) {
          b.data[i] = std::clamp(std::abs(d) * norm, 0.F, 1.F);// NOLINT
        } else {
          r.data[i] = std::clamp(d * norm, 0.F, 1.F);// NOLINT
        }
        ++p;
      }
      if (!Empty(g)) {
        g.data[i] =// NOLINT
          std::clamp(static_cast<float>(fp->demux_frame.hdr.frame_nb) / 400, 0.F, 1.F);
        ++p;
      }
      if (!Empty(a)) {
        a.data[i] = (d < 0) ? 1.F : 0.F;// NOLINT
        ++p;
      }
      if (p == 0U) { return false; }
    }
  }

  fp->mux_frame.hdr = fp->demux_frame.hdr;
  fp->mux_frame.buf = fp->demux_frame.buf.get();
  if (!ilp_movie::FillArrays(/*out*/ fp->mux_frame.data,
        /*out*/ fp->mux_frame.linesize,
        fp->mux_frame.buf,
        fp->mux_frame.hdr.pix_fmt_name,
        fp->mux_frame.hdr.width,
        fp->mux_frame.hdr.height)) {
    return false;// NOLINT
  }

  return true;
}

auto WriteFrames(const ilp_movie::MuxContext &mux_ctx, const int frame_count) -> bool
{
  if (!(frame_count > 0)) { return false; }

  for (int i = 0; i < frame_count; ++i) {
    FramePair fp{};
    if (!MakeFramePair(
          mux_ctx.params.width, mux_ctx.params.height, i, ilp_movie::PixFmt::kRGB_P_F32, &fp)) {
      return false;
    }
    if (!ilp_movie::MuxWriteFrame(mux_ctx, fp.mux_frame)) { return false; }
  }
  return ilp_movie::MuxFinish(mux_ctx);
}

struct FrameStats
{
  double r_max_err = std::numeric_limits<double>::lowest();
  double g_max_err = std::numeric_limits<double>::lowest();
  double b_max_err = std::numeric_limits<double>::lowest();
  double r_avg_err = 0.0;
  double g_avg_err = 0.0;
  double b_avg_err = 0.0;
  bool key_frame = false;
};

auto SeekFrames(ilp_movie::Decoder &decoder, const int frame_count) -> std::vector<FrameStats>
{
  if (!(frame_count >= 1)) { return {}; }

  std::vector<FrameStats> frame_stats;
  frame_stats.reserve(static_cast<std::size_t>(frame_count));

#if 1
  std::vector<int> frame_range(static_cast<std::size_t>(frame_count));
  const int start_value = 1;
  std::iota(std::begin(frame_range), std::end(frame_range), start_value);
  auto rng = std::default_random_engine{ /*seed=*/1981 };// NOLINT
  std::shuffle(std::begin(frame_range), std::end(frame_range), rng);
#else
  std::vector<int> frame_range;
  frame_range.push_back(std::max(1, frame_count / 2));
#endif

  for (const auto frame_nb : frame_range) {
    FrameStats fs{};

    ilp_movie::Frame seek_frame = {};
    if (!decoder.DecodeVideoFrame(/*stream_index=*/0, frame_nb, /*out*/ seek_frame)) { return {}; }
    if (!(seek_frame.hdr.frame_nb == frame_nb)) { return {}; }
    fs.key_frame = seek_frame.hdr.key_frame;

    FramePair fp = {};
    if (!MakeFramePair(seek_frame.hdr.width,
          seek_frame.hdr.height,
          frame_nb,
          seek_frame.hdr.pix_fmt_name,
          &fp)) {
      return {};
    }

    using ilp_movie::CompPixelData;
    namespace Comp = ilp_movie::Comp;

    // The data we read...
    const auto r_seek = CompPixelData<const float>(seek_frame.data,
      Comp::kR,
      seek_frame.hdr.width,
      seek_frame.hdr.height,
      seek_frame.hdr.pix_fmt_name);
    const auto g_seek = CompPixelData<const float>(seek_frame.data,
      Comp::kG,
      seek_frame.hdr.width,
      seek_frame.hdr.height,
      seek_frame.hdr.pix_fmt_name);
    const auto b_seek = CompPixelData<const float>(seek_frame.data,
      Comp::kB,
      seek_frame.hdr.width,
      seek_frame.hdr.height,
      seek_frame.hdr.pix_fmt_name);

    // The data we wrote...
    const auto r_mux = CompPixelData<const float>(fp.mux_frame.data,
      Comp::kR,
      fp.mux_frame.hdr.width,
      fp.mux_frame.hdr.height,
      fp.mux_frame.hdr.pix_fmt_name);
    const auto g_mux = CompPixelData<const float>(fp.mux_frame.data,
      Comp::kG,
      fp.mux_frame.hdr.width,
      fp.mux_frame.hdr.height,
      fp.mux_frame.hdr.pix_fmt_name);
    const auto b_mux = CompPixelData<const float>(fp.mux_frame.data,
      Comp::kB,
      fp.mux_frame.hdr.width,
      fp.mux_frame.hdr.height,
      fp.mux_frame.hdr.pix_fmt_name);

    if (r_seek.count != r_mux.count) { return {}; }
    for (std::size_t i = 0; i < r_seek.count; ++i) {
      const auto err = static_cast<double>(std::abs(r_seek.data[i] - r_mux.data[i]));// NOLINT
      fs.r_avg_err += err;
      fs.r_max_err = std::max(err, fs.r_max_err);
    }
    fs.r_avg_err /= static_cast<double>(r_seek.count);

    if (b_seek.count != g_mux.count) { return {}; }
    for (std::size_t i = 0; i < g_seek.count; ++i) {
      const auto err = static_cast<double>(std::abs(g_seek.data[i] - g_mux.data[i]));// NOLINT
      fs.g_avg_err += err;
      fs.g_max_err = std::max(err, fs.g_max_err);
    }
    fs.g_avg_err /= static_cast<double>(g_seek.count);

    if (b_seek.count != b_mux.count) { return {}; }
    for (std::size_t i = 0; i < b_seek.count; ++i) {
      const auto err = static_cast<double>(std::abs(b_seek.data[i] - b_mux.data[i]));// NOLINT
      fs.b_avg_err += err;
      fs.b_max_err = std::max(err, fs.b_max_err);
    }
    fs.b_avg_err /= static_cast<double>(b_seek.count);

    {// Debug.
      std::ostringstream oss;
      oss << "Frame " << frame_nb << " | "
          << "r_avg: " << fs.r_avg_err << ", r_max: " << fs.r_max_err << " | "
          << "g_avg: " << fs.g_avg_err << ", g_max: " << fs.g_max_err << " | "
          << "b_avg: " << fs.b_avg_err << ", b_max: " << fs.b_max_err << "\n";
      ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
    }
    frame_stats.push_back(fs);
  }
  return frame_stats;
}

TEST_CASE("seek(prores)")
{
  std::vector<std::string> log_lines = {};
  ilp_movie::SetLogCallback([&log_lines](int level, const char *s) {
    std::ostringstream oss;
    oss << "[ilp_movie][" << ilp_movie::LogLevelString(level) << "] " << s;
    log_lines.emplace_back(oss.str());
  });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  const auto dump_log_on_fail = [&](const bool cond) {
    if (!cond) {
      // Dump log before exiting.
      for (auto &&line : log_lines) { std::cerr << line; }
      log_lines.clear();
    }
    return cond;
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

    constexpr std::string_view kFilename = "/tmp/test_data/decoder_test.mov"sv;
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr int kFrameCount = 200;

    {// Write.
      ilp_movie::ProRes::EncodeParameters enc_params = {};
      enc_params.profile_name = ilp_movie::ProRes::ProfileName::kHq;// -profile:v 3
      auto mux_params = ilp_movie::MakeMuxParameters(kFilename,
        kWidth,
        kHeight,
        /*frame_rate=*/24.0,// -r 24
        enc_params);
      REQUIRE(dump_log_on_fail(mux_params.has_value()));
      REQUIRE(dump_log_on_fail(mux_params->pix_fmt == "yuv422p10le"sv));// -pix_fmt yuv422p10le
      mux_params->color_range = ilp_movie::ColorRange::kTv;// -color_range tv
      mux_params->colorspace = ilp_movie::Colorspace::kBt709;// -colorspace bt709
      mux_params->color_primaries = ilp_movie::Colorspace::kBt709;// -color_primaries bt709
      mux_params->color_trc = ilp_movie::ColorTrc::kIec61966_2_1;// -color_trc iec61966-2-1
      // -vf "scale=in_color_matrix=bt709:out_color_matrix=bt709"
      mux_params->filter_graph = "scale=in_color_matrix=bt709:out_color_matrix=bt709"sv;
      auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
      REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
      REQUIRE(dump_log_on_fail(WriteFrames(*mux_ctx, kFrameCount)));
      mux_ctx.reset();
    }

    {// Read.
      ilp_movie::Decoder decoder{};
      REQUIRE(dump_log_on_fail(decoder.Open(kFilename.data(),
        ilp_movie::DecoderFilterGraphDescription{
          "scale=in_color_matrix=bt709:out_color_matrix=bt709"
          ":flags=print_info+spline+accurate_rnd+full_chroma_int+full_chroma_inp",
          "gbrpf32le" })));

      auto &&vsh = decoder.VideoStreamHeaders();
      REQUIRE(dump_log_on_fail(vsh.size() == 1U));
      REQUIRE(dump_log_on_fail(vsh[0].stream_index == 0));
      REQUIRE(dump_log_on_fail(vsh[0].width == kWidth));
      REQUIRE(dump_log_on_fail(vsh[0].height == kHeight));
      REQUIRE(dump_log_on_fail(vsh[0].first_frame_nb == 1));
      REQUIRE(dump_log_on_fail(vsh[0].frame_count == 200));

      // clang-format off
      //"scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709";
      // REQUIRE(decoder.SetFilterGraph(vsh[0].stream_index,
      //   /*filter_descr=*/"scale=in_color_matrix=bt709:out_color_matrix=bt709"
      //                      ":flags=print_info+spline+accurate_rnd+full_chroma_int+full_chroma_inp"
      //                      //":flags=print_info+gauss"
      //   ));
      // clang-format on

      const auto frame_stats = SeekFrames(decoder, kFrameCount);
      REQUIRE(dump_log_on_fail(frame_stats.size() == kFrameCount));

      int bad_frame = -1;
      for (int i = 0; i < kFrameCount; ++i) {
        const auto &fs = frame_stats.at(static_cast<std::size_t>(i));
        if (!(fs.key_frame == true)) {// NOLINT
          bad_frame = i;
          break;
        }

        // clang-format off
        if (!(0.0 <= fs.r_avg_err && fs.r_avg_err < 0.01 &&
              0.0 <= fs.g_avg_err && fs.g_avg_err < 0.01 &&
              0.0 <= fs.b_avg_err && fs.b_avg_err < 0.01 &&
              0.0 <= fs.r_max_err && fs.r_max_err < 0.03 &&
              0.0 <= fs.g_max_err && fs.g_max_err < 0.03 &&
              0.0 <= fs.b_max_err && fs.b_max_err < 0.03)) {
          bad_frame = i;
          break;
        }
        // clang-format on
      }
      REQUIRE(dump_log_on_fail(bad_frame == -1));
    }
  }

  dump_log_on_fail(false);// TMP!!
}

std::once_flag write_h264{};

TEST_CASE("seek(h.264)")
{
  std::vector<std::string> log_lines = {};
  ilp_movie::SetLogCallback([&log_lines](const int level, const char *s) {
    std::string str{ s };
    if (!str.empty() && *str.rbegin() == '\n') {
      std::ostringstream oss;
      oss << "[ilp_movie][" << ilp_movie::LogLevelString(level) << "] " << str;
      str = oss.str();
    }

    log_lines.emplace_back(std::move(str));
  });
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);

  const auto dump_log_on_fail = [&](const bool cond) {
    if (!cond) {
      // Dump log before exiting.
      for (auto &&line : log_lines) { std::cerr << line; }
      log_lines.clear();
    }
    return cond;
  };

  constexpr std::string_view kFilename = "/tmp/test_data/demux_test_h264.mp4"sv;
  constexpr int kWidth = 640;
  constexpr int kHeight = 480;
  constexpr int kFrameCount = 200;
  std::call_once(write_h264, [&]() {
    // Write.
    auto mux_params = ilp_movie::MakeMuxParameters(
      kFilename, kWidth, kHeight, /*frame_rate=*/24.0, ilp_movie::H264::EncodeParameters{});
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    REQUIRE(dump_log_on_fail(WriteFrames(*mux_ctx, kFrameCount)));
    mux_ctx.reset();
  });

  SECTION("RGB")
  {
    // Read.
    ilp_movie::Decoder decoder{};
    REQUIRE(dump_log_on_fail(decoder.Open(
      kFilename.data(), ilp_movie::DecoderFilterGraphDescription{ "null", "gbrpf32le" })));

    auto &&vsh = decoder.VideoStreamHeaders();
    REQUIRE(dump_log_on_fail(vsh.size() == 1U));
    REQUIRE(dump_log_on_fail(vsh[0].stream_index == 0));
    REQUIRE(dump_log_on_fail(vsh[0].width == kWidth));
    REQUIRE(dump_log_on_fail(vsh[0].height == kHeight));
    REQUIRE(dump_log_on_fail(vsh[0].first_frame_nb == 1));
    REQUIRE(dump_log_on_fail(vsh[0].frame_count == kFrameCount));

    const auto frame_stats = SeekFrames(decoder, kFrameCount);
    REQUIRE(dump_log_on_fail(frame_stats.size() == kFrameCount));

    int bad_frame = -1;
    for (int i = 0; i < kFrameCount; ++i) {
      const auto &fs = frame_stats.at(static_cast<std::size_t>(i));

      // clang-format off
      if (!(0.0 <= fs.r_avg_err && fs.r_avg_err < 0.01 &&
            0.0 <= fs.g_avg_err && fs.g_avg_err < 0.01 &&
            0.0 <= fs.b_avg_err && fs.b_avg_err < 0.01 &&
            0.0 <= fs.r_max_err && fs.r_max_err < 0.03 &&
            0.0 <= fs.g_max_err && fs.g_max_err < 0.03 &&
            0.0 <= fs.b_max_err && fs.b_max_err < 0.03)) {
        bad_frame = i;
        break;
      }
      // clang-format on
    }
    REQUIRE(dump_log_on_fail(bad_frame == -1));
  }

  SECTION("multiple_decoders_same_file")
  {
    ilp_movie::Decoder d0{};
    ilp_movie::Decoder d1{};
    REQUIRE(dump_log_on_fail(d0.Open(kFilename.data(),
      ilp_movie::DecoderFilterGraphDescription{
        "scale=in_color_matrix=bt709:out_color_matrix=bt709"
        ":flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp, vflip",
        "gbrpf32le" })));
    REQUIRE(dump_log_on_fail(d1.Open(kFilename.data(),
      ilp_movie::DecoderFilterGraphDescription{
        "scale=w=in_w/2:h=in_h/2:flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp",
        "gbrpf32le" })));


    REQUIRE(dump_log_on_fail(d0.BestVideoStreamIndex() == 0));
    REQUIRE(dump_log_on_fail(d1.BestVideoStreamIndex() == 0));
    constexpr int stream_index = 0;

    //     REQUIRE(dump_log_on_fail(d0.SetFilterGraph(stream_index,
    // #if 0
    //       /*filter_descr=*/"scale=in_color_matrix=bt709:out_color_matrix=bt709, vflip",
    //       /*sws_flags=*/"flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp"
    // #else
    //       /*filter_descr=*/
    //       "scale=in_color_matrix=bt709:out_color_matrix=bt709:flags=spline+accurate_rnd+full_chroma_"
    //       "int+full_chroma_inp, vflip"
    // #endif
    //       )));
    //     REQUIRE(dump_log_on_fail(d1.SetFilterGraph(stream_index,
    //       /*filter_descr=*/
    //       "scale=w=in_w/2:h=in_h/2:flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp")));

    int fail0 = -1;
    int fail1 = -1;
    std::thread t0{ [&d0, &fail0]() {
      ilp_movie::Frame dec_frame{};
      for (int f = 1; f <= kFrameCount; ++f) {
        if (!d0.DecodeVideoFrame(stream_index, f, /*out*/ dec_frame)) {
          fail0 = f;
          break;
        }
      }
    } };
    std::thread t1{ [&d1, &fail1]() {
      ilp_movie::Frame dec_frame{};
      for (int f = 1; f <= kFrameCount; ++f) {
        if (!d1.DecodeVideoFrame(stream_index, f, /*out*/ dec_frame)) {
          fail1 = f;
          break;
        }

        if (!(dec_frame.hdr.width == kWidth / 2 && dec_frame.hdr.height == kHeight / 2)) {
          fail1 = f;
          break;
        }
      }
    } };

    t0.join();
    t1.join();

    REQUIRE(dump_log_on_fail(fail0 == -1));
    REQUIRE(dump_log_on_fail(fail1 == -1));
  }

  dump_log_on_fail(false);// TMP!!
}

TEST_CASE("supported formats")
{
  auto &&fmts0 = ilp_movie::Decoder::SupportedFormats();
  REQUIRE(!fmts0.empty());
}

}// namespace
