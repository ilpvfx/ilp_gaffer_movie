#include <algorithm>// std::shuffle
#include <iostream>// std::cout, std::cerr
#include <random>// std::default_random_engine
#include <sstream>// std::ostringstream
#include <string>// std::string
#include <vector>// std::vector

#include <catch2/catch_test_macros.hpp>

#include <ilp_movie/decoder.hpp>
#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>
#include <ilp_movie/pixel_data.hpp>

using namespace std::literals;// "hello"sv

namespace {

// NOTE(tohi): The mux frame channel pointers should be initialized so that they refer to the demux
//             frame buffer.
struct FramePair
{
  ilp_movie::MuxFrame mux_frame = {};
  ilp_movie::DecodedVideoFrame dec_frame = {};
};

[[nodiscard]] auto MakeFramePair(const int w,
  const int h,
  const int frame_nb,
  const ilp_movie::PixelFormat pix_fmt,
  FramePair *const fp) -> bool
{
  if (!(w > 0 && h > 0)) { return false; }

  fp->dec_frame.width = w;
  fp->dec_frame.height = h;
  fp->dec_frame.frame_nb = frame_nb;
  fp->dec_frame.pixel_aspect_ratio = { /*.num=*/1, /*.den=*/1 };
  fp->dec_frame.pix_fmt = pix_fmt;
  fp->dec_frame.buf.count = ilp_movie::ChannelCount(pix_fmt)
                            * static_cast<std::size_t>(fp->dec_frame.width)
                            * static_cast<std::size_t>(fp->dec_frame.height);
  fp->dec_frame.buf.data = std::make_unique<float[]>(fp->dec_frame.buf.count);// NOLINT

  const auto r = ilp_movie::ChannelData(&(fp->dec_frame), ilp_movie::Channel::kRed);
  const auto g = ilp_movie::ChannelData(&(fp->dec_frame), ilp_movie::Channel::kGreen);
  const auto b = ilp_movie::ChannelData(&(fp->dec_frame), ilp_movie::Channel::kBlue);
  const auto a = ilp_movie::ChannelData(&(fp->dec_frame), ilp_movie::Channel::kAlpha);

  // TMP!!
  const float cx = 0.5F * static_cast<float>(fp->dec_frame.width) + static_cast<float>(frame_nb);
  const float cy = 0.5F * static_cast<float>(fp->dec_frame.height);
  const float rad =
    0.25F * static_cast<float>(std::min(fp->dec_frame.width, fp->dec_frame.height));// in pixels
  const float norm =
    1.0F
    / (0.5F
       * static_cast<float>(std::sqrt(
         fp->dec_frame.width * fp->dec_frame.width + fp->dec_frame.height * fp->dec_frame.height)));

  for (int y = 0; y < fp->dec_frame.height; ++y) {
    for (int x = 0; x < fp->dec_frame.width; ++x) {
      const auto i =
        static_cast<size_t>(y) * static_cast<size_t>(fp->dec_frame.width) + static_cast<size_t>(x);
      const float dx = static_cast<float>(x) - cx;
      const float dy = static_cast<float>(y) - cy;
      const float d = std::sqrt(dx * dx + dy * dy) - rad;

      switch (fp->dec_frame.pix_fmt) {
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

  fp->mux_frame.width = fp->dec_frame.width;
  fp->mux_frame.height = fp->dec_frame.height;
  fp->mux_frame.frame_nb = static_cast<float>(fp->dec_frame.frame_nb);
  fp->mux_frame.r = ilp_movie::ChannelData(fp->dec_frame.width,
    fp->dec_frame.height,
    ilp_movie::Channel::kRed,
    fp->dec_frame.buf.data.get(),
    fp->dec_frame.buf.count)
                      .data;
  fp->mux_frame.g = ilp_movie::ChannelData(fp->dec_frame.width,
    fp->dec_frame.height,
    ilp_movie::Channel::kGreen,
    fp->dec_frame.buf.data.get(),
    fp->dec_frame.buf.count)
                      .data;
  fp->mux_frame.b = ilp_movie::ChannelData(fp->dec_frame.width,
    fp->dec_frame.height,
    ilp_movie::Channel::kBlue,
    fp->dec_frame.buf.data.get(),
    fp->dec_frame.buf.count)
                      .data;
  fp->mux_frame.a = ilp_movie::ChannelData(fp->dec_frame.width,
    fp->dec_frame.height,
    ilp_movie::Channel::kAlpha,
    fp->dec_frame.buf.data.get(),
    fp->dec_frame.buf.count)
                      .data;

  return true;
}

auto WriteFrames(const ilp_movie::MuxContext &mux_ctx, const int frame_count) -> bool
{
  if (!(frame_count > 0)) { return false; }

  for (int i = 0; i < frame_count; ++i) {
    FramePair fp{};
    if (!MakeFramePair(
          mux_ctx.params.width, mux_ctx.params.height, i, ilp_movie::PixelFormat::kRGB, &fp)) {
      return false;
    }
    if (!ilp_movie::MuxWriteFrame(mux_ctx, fp.mux_frame)) { return false; }
  }
  return ilp_movie::MuxFinish(mux_ctx);
}

struct SeekStats
{
  double r_max_err = std::numeric_limits<double>::lowest();
  double g_max_err = std::numeric_limits<double>::lowest();
  double b_max_err = std::numeric_limits<double>::lowest();
  double r_avg_err = 0.0;
  double g_avg_err = 0.0;
  double b_avg_err = 0.0;
  std::size_t num_key_frames = 0U;
};

auto SeekFrames(ilp_movie::Decoder &decoder, const int frame_count) -> std::optional<SeekStats>
{
  SeekStats ss{};

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
    ilp_movie::DecodedVideoFrame seek_frame = {};
    if (!decoder.DecodeVideoFrame(/*stream_index=*/0, frame_nb, /*out*/ seek_frame)) {
      return std::nullopt;
    }
    if (!(seek_frame.frame_nb == frame_nb)) { return std::nullopt; }
    if (seek_frame.key_frame) { ++ss.num_key_frames; }

    FramePair fp = {};
    if (!MakeFramePair(
          seek_frame.width, seek_frame.height, frame_nb, ilp_movie::PixelFormat::kRGB, &fp)) {
      return std::nullopt;
    }

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
    for (std::size_t i = 0; i < r.count; ++i) {
      const auto err = static_cast<double>(std::abs(r.data[i] - fp.mux_frame.r[i]));// NOLINT
      ss.r_avg_err += err;
      ss.r_max_err = std::max(err, ss.r_max_err);
    }
    ss.r_avg_err /= static_cast<double>(r.count);
    for (std::size_t i = 0; i < g.count; ++i) {
      const auto err = static_cast<double>(std::abs(g.data[i] - fp.mux_frame.g[i]));// NOLINT
      ss.g_avg_err += err;
      ss.g_max_err = std::max(err, ss.g_max_err);
    }
    ss.g_avg_err /= static_cast<double>(g.count);
    for (std::size_t i = 0; i < b.count; ++i) {
      const auto err = static_cast<double>(std::abs(b.data[i] - fp.mux_frame.b[i]));// NOLINT
      ss.b_avg_err += err;
      ss.b_max_err = std::max(err, ss.b_max_err);
    }
    ss.b_avg_err /= static_cast<double>(b.count);

    {// Debug.
      std::ostringstream oss;
      oss << "Frame " << frame_nb << "\n"
          << "r | avg: " << ss.r_avg_err << ", max: " << ss.r_max_err << "\n"
          << "g | avg: " << ss.g_avg_err << ", max: " << ss.g_max_err << "\n"
          << "b | avg: " << ss.b_avg_err << ", max: " << ss.b_max_err << "\n";
      ilp_movie::LogMsg(ilp_movie::LogLevel::kInfo, oss.str().c_str());
    }
  }
  return ss;
}

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
      std::unique_ptr<ilp_movie::Decoder> decoder;
      try {
        decoder = std::make_unique<ilp_movie::Decoder>(std::string{ kFilename });
      } catch (std::exception &) {
        REQUIRE(dump_log_on_fail(false));
      }

      std::vector<ilp_movie::DecodeVideoStreamInfo> stream_info = decoder->StreamInfo();
      REQUIRE(dump_log_on_fail(stream_info.size() == 1U));
      REQUIRE(dump_log_on_fail(stream_info[0].stream_index == 0));
      REQUIRE(dump_log_on_fail(stream_info[0].is_best == true));
      REQUIRE(dump_log_on_fail(stream_info[0].width == kWidth));
      REQUIRE(dump_log_on_fail(stream_info[0].height == kHeight));
      REQUIRE(dump_log_on_fail(stream_info[0].first_frame_nb == 1));
      REQUIRE(dump_log_on_fail(stream_info[0].frame_count == 200));

      //"scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709";
      REQUIRE(decoder->SetStreamFilterGraph(stream_info[0].stream_index,
        /*filter_descr=*/"scale=in_color_matrix=bt709:out_color_matrix=bt709"));

      const auto ss = SeekFrames(*decoder, kFrameCount);
      REQUIRE(dump_log_on_fail(ss.has_value()));
      REQUIRE(dump_log_on_fail(
        ss->num_key_frames == kFrameCount));// ProRes is all intraframe, so all key frames!
      REQUIRE(dump_log_on_fail(0.0 <= ss->r_avg_err && ss->r_avg_err < 0.01));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->g_avg_err && ss->g_avg_err < 0.01));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->b_avg_err && ss->b_avg_err < 0.01));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->r_max_err && ss->r_max_err < 0.15));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->g_max_err && ss->g_max_err < 0.15));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->b_max_err && ss->b_max_err < 0.15));// NOLINT
      decoder.reset();
    }
  }

  // dump_log_on_fail(false);// TMP!!
}

TEST_CASE("seek - h.264")
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

  SECTION("RGB")
  {
    const char *const kFilename = "/tmp/test_data/demux_test_h264.mp4";
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr int kFrameCount = 200;

    {// Write.
      auto mux_params = ilp_movie::MakeMuxParameters(
        kFilename, kWidth, kHeight, 24.0, ilp_movie::H264::EncodeParameters{});
      REQUIRE(dump_log_on_fail(mux_params.has_value()));
      auto mux_ctx = ilp_movie::MakeMuxContext(*mux_params);
      REQUIRE(dump_log_on_fail(mux_ctx != nullptr));
      REQUIRE(dump_log_on_fail(WriteFrames(*mux_ctx, kFrameCount)));
      mux_ctx.reset();
    }

    {// Read.
      std::unique_ptr<ilp_movie::Decoder> decoder;
      try {
        decoder = std::make_unique<ilp_movie::Decoder>(std::string{ kFilename });
      } catch (std::exception &) {
        REQUIRE(dump_log_on_fail(false));
      }

      std::vector<ilp_movie::DecodeVideoStreamInfo> stream_info = decoder->StreamInfo();
      REQUIRE(dump_log_on_fail(stream_info.size() == 1U));
      REQUIRE(dump_log_on_fail(stream_info[0].stream_index == 0));
      REQUIRE(dump_log_on_fail(stream_info[0].is_best == true));
      REQUIRE(dump_log_on_fail(stream_info[0].width == kWidth));
      REQUIRE(dump_log_on_fail(stream_info[0].height == kHeight));
      REQUIRE(dump_log_on_fail(stream_info[0].first_frame_nb == 1));
      REQUIRE(dump_log_on_fail(stream_info[0].frame_count == kFrameCount));

      const auto ss = SeekFrames(*decoder, kFrameCount);
      REQUIRE(dump_log_on_fail(ss.has_value()));
      REQUIRE(dump_log_on_fail(0.0 <= ss->r_avg_err && ss->r_avg_err < 0.01));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->g_avg_err && ss->g_avg_err < 0.01));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->b_avg_err && ss->b_avg_err < 0.01));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->r_max_err && ss->r_max_err < 0.15));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->g_max_err && ss->g_max_err < 0.15));// NOLINT
      REQUIRE(dump_log_on_fail(0.0 <= ss->b_max_err && ss->b_max_err < 0.15));// NOLINT
      decoder.reset();
    }
  }

  dump_log_on_fail(false);// TMP!!
}

}// namespace