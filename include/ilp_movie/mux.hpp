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

#include <ilp_movie/ilp_movie_export.hpp>// ILP_MOVIE_EXPORT

namespace ilp_movie {

namespace ProRes {

  struct Config
  {
    [[nodiscard]] static constexpr auto Preset(const std::string_view name,
      const bool alpha = false) noexcept -> std::optional<Config>
    {
      Config cfg = {};

      // 4:2:2 profiles (no alpha support).
      if (name == "proxy") {
        if (alpha) { return std::nullopt; }
        cfg.name = "proxy";
        cfg.profile = 0;
        cfg.pix_fmt = "yuv422p10le";
      } else if (name == "lt") {
        if (alpha) { return std::nullopt; }
        cfg.name = "lt";
        cfg.profile = 1;
        cfg.pix_fmt = "yuv422p10le";
      } else if (name == "standard") {
        if (alpha) { return std::nullopt; }
        cfg.name = "standard";
        cfg.profile = 2;
        cfg.pix_fmt = "yuv422p10le";
      } else if (name == "hq") {
        if (alpha) { return std::nullopt; }
        cfg.name = "hq";
        cfg.profile = 3;
        cfg.pix_fmt = "yuv422p10le";
      }
      // 4:4:4 profiles (optional alpha).
      else if (name == "4444") {
        cfg.name = "4444";
        cfg.profile = 4;
        cfg.pix_fmt = alpha ? "yuva444p10le" : "yuv444p10le";
      } else if (name == "4444xq") {
        cfg.name = "4444xq";
        cfg.profile = 5;
        cfg.pix_fmt = alpha ? "yuva444p10le" : "yuv444p10le";
      }
      // Unrecognized profile name.
      else {
        return std::nullopt;
      }

      return cfg;
    }

    // The basic difference is the bitrates...
    const char *name = "hq";
    int profile = 3;

    // Supported pixel formats (depending on profile).
    // NOTE: We are only considering 10-bit pixel formats for now.
    const char *pix_fmt = "yuv422p10le";

    // Values between 9 - 13 give a good results.
    // Lower value gives higher quality, but larger file size.
    int qscale = 11;

    // Tricks QuickTime and Final Cut Pro into thinking that the movie was generated using a
    // QuickTime ProRes encoder.
    const char *vendor = "apl0";
  };

}// namespace ProRes

namespace h264 {

  // The available presets in descending order of speed are:
  namespace Preset {
    // clang-format off
    constexpr const char *kUltrafast = "ultrafast";
    constexpr const char *kSuperfast = "superfast";
    constexpr const char *kVeryfast  = "veryfast";
    constexpr const char *kFaster    = "faster";
    constexpr const char *kFast      = "fast";
    constexpr const char *kMedium    = "medium";
    constexpr const char *kSlow      = "slow";
    constexpr const char *kSlower    = "slower";
    constexpr const char *kVeryslow  = "veryslow";
    // clang-format on
  }// namespace Preset

  // Supported pixel formats.
  namespace PixelFormat {
    // clang-format off
    constexpr const char *kYuv420p   = "yuv420p";
    constexpr const char *kYuv420p10 = "yuv420p10";
    constexpr const char *kYuv422p   = "yuv422p";
    constexpr const char *kYuv422p10 = "yuv422p10";
    constexpr const char *kYuv444p   = "yuv444p";
    constexpr const char *kYuv444p10 = "yuv444p10";
    // clang-format on
  }// namespace PixelFormat

  // Optional tune settings.
  namespace Tune {
    // clang-format off
    constexpr const char *kNone        = nullptr;
    constexpr const char *kFilm        = "film";
    constexpr const char *kAnimation   = "animation";
    constexpr const char *kGrain       = "grain";
    constexpr const char *kStillimage  = "stillimage";
    constexpr const char *kFastdecode  = "fastdecode";
    constexpr const char *kZerolatency = "zerolatency";
    // clang-format on
  }// namespace Tune


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
    // For x642 sane values are in the range [18, 28].
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

}// namespace h264

struct MuxImpl;
using mux_impl_ptr = std::unique_ptr<MuxImpl, std::function<void(MuxImpl *)>>;

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
// similar for 'g' and 'b'.
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

// Finalize the file by flushing streams and possibly performing other operations.
//
// Returns true if successful, otherwise false.
// Note that the MuxContext must be manually free'd using MuxFreeImpl{} regardless of success.
[[nodiscard]] ILP_MOVIE_EXPORT auto MuxFinish(const MuxContext &mux_ctx) noexcept -> bool;

}// namespace ilp_movie
