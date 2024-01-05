#include <algorithm>// std::shuffle
#include <array>// std::array
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

// NOTE(tohi): The frame view should be initialized such that it
//             references the frame's buffer.
struct FramePair
{
  ilp_movie::FrameView mux_frame = {};
  ilp_movie::Frame demux_frame = {};
};

// Use with:
// - FrameT = ilp_movie::Frame
// - FrameT = ilp_movie::FrameView
template<typename PixelT, typename FrameT>
[[nodiscard]] auto CompPixelData(const FrameT &frame,
  const ilp_movie::Comp::ValueType comp) noexcept -> ilp_movie::PixelData<PixelT>
{
  return ilp_movie::CompPixelData<PixelT>(
    frame.data, frame.linesize, comp, frame.hdr.height, frame.hdr.pix_fmt_name);
}

// NOTE(tohi): Makes RGB frames for now, could add possibility to
//             make RGBA frames if necessary.
[[nodiscard]] auto MakeFramePair(const int w,
  const int h,
  const int frame_nb,
  const char *pix_fmt_name,
  FramePair &fp) -> bool
{
  if (!(w > 0 && h > 0 && pix_fmt_name != nullptr)) { return false; }

  const auto buf_size = ilp_movie::GetBufferSize(pix_fmt_name, w, h);
  if (!buf_size.has_value()) { return false; }

  // "Read" frame.
  fp.demux_frame.hdr.width = w;
  fp.demux_frame.hdr.height = h;
  fp.demux_frame.hdr.frame_nb = frame_nb;
  fp.demux_frame.hdr.pix_fmt_name = pix_fmt_name;
  fp.demux_frame.buf = std::make_unique<uint8_t[]>(*buf_size);// NOLINT
  if (!ilp_movie::FillArrays(/*out*/ fp.demux_frame.data,
        /*out*/ fp.demux_frame.linesize,
        fp.demux_frame.buf.get(),
        fp.demux_frame.hdr.pix_fmt_name,
        fp.demux_frame.hdr.width,
        fp.demux_frame.hdr.height)) {
    return false;
  }

  // "Write" frame.
  fp.mux_frame.hdr = fp.demux_frame.hdr;
  fp.mux_frame.buf = fp.demux_frame.buf.get();
  if (!ilp_movie::FillArrays(/*out*/ fp.mux_frame.data,
        /*out*/ fp.mux_frame.linesize,
        fp.mux_frame.buf,
        fp.mux_frame.hdr.pix_fmt_name,
        fp.mux_frame.hdr.width,
        fp.mux_frame.hdr.height)) {
    return false;
  }

  // NOTE(tohi): Make some weird psychedelic gradients... just for stress-testing
  //             the encoders/decoders.
  const auto color_func = [w, h, frame_nb](int x, int y) -> std::array<float, 4> {
    const float cx = 0.5F * static_cast<float>(w) + static_cast<float>(frame_nb);
    const float cy = 0.5F * static_cast<float>(h);
    // In [pixels]
    const float rad = 0.25F * static_cast<float>(std::min(w, h));
    const float norm = 1.0F / (0.5F * static_cast<float>(std::sqrt(w * w + h * h)));
    const float dx = static_cast<float>(x) - cx;
    const float dy = static_cast<float>(y) - cy;
    const float d = std::sqrt(dx * dx + dy * dy) - rad;

    std::array<float, 4> rgba{};
    if (d < 0.F) {
      rgba[0] = 0.F;
      rgba[2] = std::clamp(std::abs(d) * norm, 0.F, 1.F);
    } else {
      rgba[0] = std::clamp(d * norm, 0.F, 1.F);
      rgba[2] = 0.F;
    }
    rgba[1] = std::clamp(static_cast<float>(frame_nb) / 400, 0.F, 1.F);
    rgba[3] = (d < 0.F) ? 1.F : 0.F;
    return rgba;
  };

  const auto r = CompPixelData<float>(fp.demux_frame, ilp_movie::Comp::kR);
  const auto g = CompPixelData<float>(fp.demux_frame, ilp_movie::Comp::kG);
  const auto b = CompPixelData<float>(fp.demux_frame, ilp_movie::Comp::kB);
  const auto a = CompPixelData<float>(fp.demux_frame, ilp_movie::Comp::kA);

  // Must have at least RGB.
  const std::size_t pixel_count = static_cast<std::size_t>(fp.demux_frame.hdr.width)
                                  * static_cast<std::size_t>(fp.demux_frame.hdr.height);
  const bool have_alpha = !Empty(a);
  if (Empty(r) || Empty(g) || Empty(b)) { return false; }
  if (!(r.count == pixel_count && r.count == g.count && r.count == b.count)) { return false; }
  if (have_alpha && !(a.count == pixel_count)) { return false; }

  for (int y = 0; y < fp.demux_frame.hdr.height; ++y) {
    for (int x = 0; x < fp.demux_frame.hdr.width; ++x) {
      const int i = y * fp.demux_frame.hdr.width + x;
      const auto rgba = color_func(x, y);
      r.data[i] = rgba[0];// NOLINT
      g.data[i] = rgba[1];// NOLINT
      b.data[i] = rgba[2];// NOLINT
      if (have_alpha) { a.data[i] = rgba[3]; }// NOLINT
    }
  }

  return true;
}

auto WriteFrames(const ilp_movie::MuxContext &mux_ctx,
  const int frame_count,
  const char *pix_fmt_name) -> bool
{
  if (!(frame_count > 0)) { return false; }

  for (int i = 0; i < frame_count; ++i) {
    FramePair fp{};
    if (!MakeFramePair(mux_ctx.params.width, mux_ctx.params.height, i, pix_fmt_name, /*out*/ fp)) {
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

auto SeekFrames(ilp_movie::Decoder &decoder, const int stream_index, const int frame_count)
  -> std::vector<FrameStats>
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
    if (!decoder.DecodeVideoFrame(stream_index, frame_nb, /*out*/ seek_frame)) { return {}; }
    if (!(seek_frame.hdr.frame_nb == frame_nb)) { return {}; }
    fs.key_frame = seek_frame.hdr.key_frame;

    FramePair fp = {};
    if (!MakeFramePair(seek_frame.hdr.width,
          seek_frame.hdr.height,
          frame_nb,
          seek_frame.hdr.pix_fmt_name,
          /*out*/ fp)) {
      return {};
    }

    // The data we read...
    const auto r_seek = CompPixelData<const float>(seek_frame, ilp_movie::Comp::kR);
    const auto g_seek = CompPixelData<const float>(seek_frame, ilp_movie::Comp::kG);
    const auto b_seek = CompPixelData<const float>(seek_frame, ilp_movie::Comp::kB);

    // The data we wrote...
    const auto r_mux = CompPixelData<const float>(fp.mux_frame, ilp_movie::Comp::kR);
    const auto g_mux = CompPixelData<const float>(fp.mux_frame, ilp_movie::Comp::kG);
    const auto b_mux = CompPixelData<const float>(fp.mux_frame, ilp_movie::Comp::kB);

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

#if 0
    {// Debug.
      std::ostringstream oss;
      oss << "Frame " << frame_nb << " | "
          << "r_avg: " << fs.r_avg_err << ", r_max: " << fs.r_max_err << " | "
          << "g_avg: " << fs.g_avg_err << ", g_max: " << fs.g_max_err << " | "
          << "b_avg: " << fs.b_avg_err << ", b_max: " << fs.b_max_err << "\n";
      ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
    }
#endif

    frame_stats.push_back(fs);
  }
  return frame_stats;
}

std::once_flag write_prores_once{};
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

  constexpr std::string_view kFilename = "/tmp/test_data/demux_test_prores.mov"sv;
  constexpr int kWidth = 640;
  constexpr int kHeight = 480;
  constexpr int kFrameCount = 200;
  std::call_once(write_prores_once, [&]() {
    // Write.
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
    REQUIRE(dump_log_on_fail(WriteFrames(*mux_ctx, kFrameCount, ilp_movie::PixFmt::kRGB_P_F32)));
    mux_ctx.reset();
  });

  SECTION("RGB")
  {
    // Read.
    ilp_movie::Decoder decoder{};
    REQUIRE(dump_log_on_fail(decoder.Open(kFilename.data(),
      ilp_movie::DecoderFilterGraphDescription{
        "scale=in_color_matrix=bt709:out_color_matrix=bt709"
        ":flags=print_info+spline+accurate_rnd+full_chroma_int+full_chroma_inp",
        ilp_movie::PixFmt::kRGB_P_F32 })));

    auto &&vsh = decoder.VideoStreamHeaders();
    REQUIRE(dump_log_on_fail(vsh.size() == 1U));
    REQUIRE(dump_log_on_fail(vsh[0].stream_index == 0));
    REQUIRE(dump_log_on_fail(vsh[0].width == kWidth));
    REQUIRE(dump_log_on_fail(vsh[0].height == kHeight));
    REQUIRE(dump_log_on_fail(vsh[0].first_frame_nb == 1));
    REQUIRE(dump_log_on_fail(vsh[0].frame_count == 200));

#if 0
      // clang-format off
      //"scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709";
      // REQUIRE(decoder.SetFilterGraph(vsh[0].stream_index,
      //   /*filter_descr=*/"scale=in_color_matrix=bt709:out_color_matrix=bt709"
      //                      ":flags=print_info+spline+accurate_rnd+full_chroma_int+full_chroma_inp"
      //                      //":flags=print_info+gauss"
      //   ));
      // clang-format on
#endif

    const auto frame_stats = SeekFrames(decoder, /*stream_index=*/0, kFrameCount);
    REQUIRE(dump_log_on_fail(frame_stats.size() == kFrameCount));

    int bad_frame = -1;
    for (int i = 0; i < kFrameCount; ++i) {
      const auto &fs = frame_stats.at(static_cast<std::size_t>(i));
      // ProRes should be all I-frames (key frames).
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

  // dump_log_on_fail(false);// TMP!!
}

std::once_flag write_h264_once{};
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
  std::call_once(write_h264_once, [&]() {
    // Write.
    auto mux_params = ilp_movie::MakeMuxParameters(
      kFilename, kWidth, kHeight, /*frame_rate=*/24.0, ilp_movie::H264::EncodeParameters{});
    REQUIRE(dump_log_on_fail(mux_params.has_value()));
    auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
    REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
    REQUIRE(dump_log_on_fail(WriteFrames(*mux_ctx, kFrameCount, ilp_movie::PixFmt::kRGB_P_F32)));
    mux_ctx.reset();
  });

  SECTION("RGB")
  {
    // Read.
    ilp_movie::Decoder decoder{};
    REQUIRE(dump_log_on_fail(decoder.Open(kFilename.data(),
      ilp_movie::DecoderFilterGraphDescription{ "null", ilp_movie::PixFmt::kRGB_P_F32 })));

    auto &&hdrs = decoder.VideoStreamHeaders();
    REQUIRE(dump_log_on_fail(hdrs.size() == 1U));
    REQUIRE(dump_log_on_fail(hdrs[0].stream_index == 0));
    REQUIRE(dump_log_on_fail(hdrs[0].width == kWidth));
    REQUIRE(dump_log_on_fail(hdrs[0].height == kHeight));
    REQUIRE(dump_log_on_fail(hdrs[0].first_frame_nb == 1));
    REQUIRE(dump_log_on_fail(hdrs[0].frame_count == kFrameCount));

    const auto frame_stats = SeekFrames(decoder, /*stream_index=*/0, kFrameCount);
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
        ilp_movie::PixFmt::kRGB_P_F32 })));
    REQUIRE(dump_log_on_fail(d1.Open(kFilename.data(),
      ilp_movie::DecoderFilterGraphDescription{
        "scale=w=in_w/2:h=in_h/2:flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp",
        ilp_movie::PixFmt::kRGB_P_F32 })));


    REQUIRE(dump_log_on_fail(d0.BestVideoStreamIndex() == 0));
    REQUIRE(dump_log_on_fail(d1.BestVideoStreamIndex() == 0));
    constexpr int stream_index = 0;

#if 0
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
#endif

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

  // dump_log_on_fail(false);// TMP!!
}

TEST_CASE("supported formats")
{
  auto &&fmts0 = ilp_movie::Decoder::SupportedFormats();
  REQUIRE(!fmts0.empty());
}

}// namespace
