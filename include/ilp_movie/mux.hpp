// Example:
// {
//   // Create and configure mux context.
//   MuxContext mux_ctx = {};
//   mux_ctx.width = 480;
//   mux_ctx.height = 340;
//
//   if (!MuxInit(&mux_ctx)) {
//     // Something went wrong, bailing...
//     // No need to free implementation here.
//     return false;
//   }
//
//   // Write some frames.
//   MuxFrame f = {};
//   f.width = mux_ctx.width;
//   f.height = mux_ctx.height;
//   for (auto&& frame : frames) {
//      f.frame_nb = frame.time;
//      f.r = frame.r_buf.data();
//      f.g = frame.g_buf.data();
//      f.b = frame.b_buf.data();
//      if (!MuxWrite(mux_ctx, f)) {
//        // Something went wrong, bailing...
//        MuxFree(&mux_ctx);
//        return false;
//      }
//   }
//
//   // Finalize the video stream.
//   if (!MuxFinish(mux_ctx)) {
//     // Something went wrong, bailing...
//     MuxFree(&mux_ctx);
//     return false;
//   }
//
//   MuxFree(&mux_ctx.impl);
//   return true;
// }

#pragma once

#include <functional>// std::function
#include <memory>// std::unique_ptr
#include <optional>// std::optional
#include <string_view>// std::string_view

#include "ilp_movie/frame.hpp"
#include "ilp_movie/ilp_movie_export.hpp"// ILP_MOVIE_EXPORT

namespace ilp_movie {

#if 0
// TODO(tohi): Re-write the muxer interface to be a class instead, similar to how the decoder (which should
//             be called demuxer) is structured. This is much easier to work with.

class MuxerImpl;
class ILP_MOVIE_EXPORT Muxer
{
public:
  Muxer();
  ~Muxer();

  // Movable.
  Muxer(Muxer &&rhs) noexcept = default;
  Muxer &operator=(Muxer &&rhs) noexcept = default;

  // Not copyable.
  Muxer(const Muxer &rhs) = delete;
  Muxer &operator=(const Muxer &rhs) = delete;

  [[nodiscard]] auto Open(const std::string &url) -> bool;

  [[nodiscard]] auto EncodeVideoFrame(int stream_index, const FrameView& frame_view) noexcept -> bool;
};
#endif


namespace ProRes {
  namespace ProfileName {
    // clang-format off
    constexpr auto kProxy    = std::string_view{"proxy"};
    constexpr auto kLt       = std::string_view{"lt"};
    constexpr auto kStandard = std::string_view{"standard"};
    constexpr auto kHq       = std::string_view{"hq"};
    constexpr auto k4444     = std::string_view{"4444"};
    constexpr auto k4444xq   = std::string_view{"4444xq"};
    // clang-format on
  }// namespace ProfileName

  struct EncodeParameters
  {
    std::string_view profile_name = ProfileName::kHq;
    int qscale = 11;
    bool alpha = false;
    std::string_view color_range = ColorRange::kPc;
    std::string_view color_primaries = ColorPrimaries::kBt709;
    std::string_view colorspace = Colorspace::kBt709;
    std::string_view color_trc = ColorTrc::kIec61966_2_1;

    // https://ffmpeg.org/ffmpeg-codecs.html#ProRes
#if 0
    // Select quantization matrix.
    // 0: ‘auto’
    // 1: ‘default’
    // 2: ‘proxy’
    // 3: ‘lt’
    // 4: ‘standard’
    // 5: ‘hq’
    //
    // If set to auto, the matrix matching the profile will be picked. If not set, the matrix
    // providing the highest quality, default, will be picked.
    int quant_mat = 0;

    // How many bits to allot for coding one macroblock. Different profiles use between 200 and
    // 2400 bits per macroblock, the maximum is 8000.
    int bits_per_mb;

    // Number of macroblocks in each slice (1-8); the default value (8) should be good in almost all
    // situations.
    int mbs_per_slice = 8;

    // Override the 4-byte vendor ID. A custom vendor ID like "apl0" would claim the stream was
    // produced by the Apple encoder.
    std::string_view vendor = std::string_view{ "apl0" };

    // Specify number of bits for alpha component. Possible values are 0, 8 and 16. Use 0 to disable
    // alpha plane coding.
    int alpha_bits = 0
#endif
  };
}// namespace ProRes

namespace H264 {
  // Supported pixel formats.
  namespace PixelFormat {
    // clang-format off
    constexpr auto kYuv420p   = std::string_view{"yuv420p"};
    constexpr auto kYuv420p10 = std::string_view{"yuv420p10"};
    constexpr auto kYuv422p   = std::string_view{"yuv422p"};
    constexpr auto kYuv422p10 = std::string_view{"yuv422p10"};
    constexpr auto kYuv444p   = std::string_view{"yuv444p"};
    constexpr auto kYuv444p10 = std::string_view{"yuv444p10"};
    // clang-format on
  }// namespace PixelFormat

  // The available presets in descending order of speed are:
  namespace Preset {
    // clang-format off
    constexpr auto kUltrafast = std::string_view{"ultrafast"};
    constexpr auto kSuperfast = std::string_view{"superfast"};
    constexpr auto kVeryfast  = std::string_view{"veryfast"};
    constexpr auto kFaster    = std::string_view{"faster"};
    constexpr auto kFast      = std::string_view{"fast"};
    constexpr auto kMedium    = std::string_view{"medium"};
    constexpr auto kSlow      = std::string_view{"slow"};
    constexpr auto kSlower    = std::string_view{"slower"};
    constexpr auto kVeryslow  = std::string_view{"veryslow"};
    // clang-format on
  }// namespace Preset

  // Optional tune settings.
  namespace Tune {
    // clang-format off
    constexpr auto kNone        = std::string_view{"__none__"};
    constexpr auto kFilm        = std::string_view{"film"};
    constexpr auto kAnimation   = std::string_view{"animation"};
    constexpr auto kGrain       = std::string_view{"grain"};
    constexpr auto kStillimage  = std::string_view{"stillimage"};
    constexpr auto kFastdecode  = std::string_view{"fastdecode"};
    constexpr auto kZerolatency = std::string_view{"zerolatency"};
    // clang-format on
  }// namespace Tune

  struct EncodeParameters
  {
    std::string_view pix_fmt = PixelFormat::kYuv420p;
    std::string_view preset = Preset::kSlower;
    std::string_view tune = Tune::kNone;
    std::string_view x264_params = std::string_view{ "keyint=15:no-deblock=1" };
    int crf = 23;
    std::string_view color_range = ColorRange::kPc;
    std::string_view color_primaries = ColorPrimaries::kBt709;
    std::string_view colorspace = Colorspace::kBt709;
    std::string_view color_trc = ColorTrc::kIec61966_2_1;
  };

#if 0
  struct Config
  {
// NOTE(tohi): Setting pixel format seems to automatically select
//             the appropriate profile, so we don't set the profile
//             explicitly.
#if 0 
    // Typically not used according to docs, leave as null to use default.
    // Supported profiles:
    //   "baseline"
    //   "main"
    //   "high"
    //   "high10"  (first 10-bit compatible profile)
    //   "high422" (supports yuv420p, yuv422p, yuv420p10le and yuv422p10le)
    //   "high444" (supports as above as well as yuv444p and yuv444p10le)
    const char *profile = nullptr;
#endif

    const char *preset = Preset::kSlower;
    const char *pix_fmt = PixelFormat::kYuv420p;
    const char *tune = Tune::kNone;

    // Constant rate factor.
    // Should be in range [0, 51], where lower values result
    // in higher quality but larger files, and vice versa.
    // For x264 sane values are in the range [18, 28].
    // A change of +/-6 should result in about half/double the file size, but this is
    // just an estimate.
    // Choose the highest CRF value that still provides an acceptable quality. If the output looks
    // good, then try a higher value. If it looks bad, choose a lower value.
    //
    // See: https://slhck.info/video/2017/02/24/crf-guide.html
    int crf = 23;

    // NOTE(tohi): I've seen mixed reports on using the following parameters:
    //
    // int qp = 23;
    // int qscale = 9;
    //
    // but only using CRF seems to be the way to go, for now.

    const char *x264_params = "keyint=15:no-deblock=1";
  };
#endif

}// namespace H264

struct MuxImpl;
using mux_impl_ptr = std::unique_ptr<MuxImpl, std::function<void(MuxImpl *)>>;

struct MuxParameters
{
  // The filename cannot be empty.
  std::string filename;

  // Pixel resolution dimensions.
  int width = -1;
  int height = -1;

  // Frames per second (FPS).
  double frame_rate = -1.0;

  // Specifies the codec to use for the video stream.
  std::string codec_name;

  std::string pix_fmt;

  // MP4 metadata.
  std::string color_range;
  std::string color_primaries;
  std::string colorspace;
  std::string color_trc;

  std::string filter_graph;

  // ===============
  // H.264 parameters.
  // ===============
  // NOTE(tohi): Setting pixel format seems to automatically select
  //             the appropriate profile, so we don't set the profile
  //             explicitly.
#if 0 
  // Typically not used according to docs, leave empty to use default.
  // Supported profiles:
  //   "baseline"
  //   "main"
  //   "high"
  //   "high10"  (first 10-bit compatible profile)
  //   "high422" (supports yuv420p, yuv422p, yuv420p10le and yuv422p10le)
  //   "high444" (supports as above, as well as yuv444p and yuv444p10le)
  std::string profile;
#endif

  std::string preset;
  std::string tune;
  std::string x264_params;

  // Constant rate factor.
  // Should be in range [0, 51], where lower values result
  // in higher quality but larger files, and vice versa.
  // For x264 sane values are in the range [18, 28].
  // A change of +/-6 should result in about half/double the file size, but this is
  // just an estimate.
  // Choose the highest CRF value that still provides an acceptable quality. If the output looks
  // good, then try a higher value. If it looks bad, choose a lower value.
  //
  // See: https://slhck.info/video/2017/02/24/crf-guide.html
  //
  // NOTE(tohi): I've seen mixed reports on using the following parameters:
  //
  // int qp = 23;
  // int qscale = 9;
  //
  // but only using CRF seems to be the way to go, for now.
  int crf = -1;

  // ===============
  // ProRes parameters.
  // ===============

  int profile = -1;

  // Values between 9 - 13 give a good results.
  // Lower value gives higher quality, but larger file size.
  int qscale = -1;

  std::string vendor;
};

#if 0
struct MuxParameters
{
  // The filename cannot be null.
  // If format_name is null the output format is guessed according to the file extension.
  const char *filename = nullptr;
  const char *format_name = nullptr;

  // Pixel resolution dimensions - must be a multiple of two.
  int width = -1;
  int height = -1;
  double fps = 24.0;

  const char *codec_name = nullptr;

  // NOTE(tohi): The fields below are (probably) only relevant for mp4 containers...
  // TODO(tohi): Shouldn't this be synced to what we pass to the filter graph??
  // mp4 metadata below!
  struct
  {
    // Supported color ranges:
    //   "unknown"
    //   "tv" (limited range)
    //   "pc" (full range)
    const char *color_range = "pc";

    const char *color_primaries = "bt709";
    const char *color_trc = "iec61966-2-1";
    const char *colorspace = "bt709";
  } mp4_metadata = {};

  const char *sws_flags = "flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp";
  const char *filtergraph =
    "scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709";

  struct
  {
    ProRes::Config cfg = ProRes::Config::Preset("hq").value();
  } pro_res = {};

  // Docs: https://trac.ffmpeg.org/wiki/Encode/H.264
  struct
  {
    h264::Config cfg = {};
  } h264 = {};
};
#endif

//
struct MuxContext
{
  // A copy of the parameters used to create the context.
  MuxParameters params = {};

  // Private implementation specific data stored as an opaque pointer.
  // The implementation specific data contains low-level resources such as
  // format context, codecs, etc. The resources are automatically freed when this
  // context is destroyed.
  mux_impl_ptr impl = nullptr;
};

// Describes how pixel data is passed to the muxer.
// Here the color channels are separated into planes, i.e. the RGB values are not interleaved.
// Instead each color channel has its own linear buffer.
// The 'r' pointer is assumed to point to a buffer of (width * height) floats, and
// similar for 'g' and 'b' (and possibly 'a').
// NOTE: Set the width and height using the corresponding values on the mux context.
struct MuxFrame
{
  int width = -1;
  int height = -1;
  float frame_nb = -1.F;
  const float *r = nullptr;
  const float *g = nullptr;
  const float *b = nullptr;
  const float *a = nullptr;
};

[[nodiscard]] ILP_MOVIE_EXPORT auto MakeMuxParameters(std::string_view filename,
  int width,
  int height,
  double frame_rate,
  const ProRes::EncodeParameters &enc_params) noexcept -> std::optional<MuxParameters>;

[[nodiscard]] ILP_MOVIE_EXPORT auto MakeMuxParameters(std::string_view filename,
  int width,
  int height,
  double frame_rate,
  const H264::EncodeParameters &enc_params) noexcept -> std::optional<MuxParameters>;

// Initialize the internal resources required to start encoding video frames.
[[nodiscard]] ILP_MOVIE_EXPORT auto MakeMuxContext(const MuxParameters &params) noexcept
  -> std::unique_ptr<MuxContext>;

// Send a frame to the MuxContext encoder. The pixel data interface is determined by the MuxFrame
// struct.
//
// Successful calls return WriteStatus::kEncodingOngoing, errors are reported by returning
// WriteStatus::kEncodingError.
//
// Note that the frame's width/height must match the corresponding values on the MuxContext since
// we currently don't support pixel scaling.
[[nodiscard]] ILP_MOVIE_EXPORT auto MuxWriteFrame(const MuxContext &mux_ctx,
  const MuxFrame &frame) noexcept -> bool;

[[nodiscard]] ILP_MOVIE_EXPORT auto MuxWriteFrame(const MuxContext &mux_ctx,
  const FrameView &frame) noexcept -> bool;


// Finalize the file by flushing streams and possibly performing other operations.
//
// Returns true if successful, otherwise false.
[[nodiscard]] ILP_MOVIE_EXPORT auto MuxFinish(const MuxContext &mux_ctx) noexcept -> bool;

}// namespace ilp_movie
