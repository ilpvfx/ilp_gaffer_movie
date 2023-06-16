#pragma once

#include <functional>// std::function

#include <ilp_movie/ilp_movie_export.hpp>

namespace ilp_movie {

struct MuxImpl;

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
//
struct MuxContext
{
  // The filename cannot be null.
  // If format_name is null the output format is guessed according to the file extension.
  const char *filename = nullptr;
  const char *format_name = nullptr;

  // Pixel resolution dimensions must be a multiple of two.
  int width = 352;
  int height = 288;
  double fps = 24.0;

  const char *codec_name = nullptr;

  // Supported color ranges:
  //   "unknown"
  //   "tv" (limited range)
  //   "pc" (full range)
  const char *color_range = "pc";

  const char *color_primaries = "bt709";
  const char *color_trc = "iec61966-2-1";
  const char *colorspace = "bt709";

  const char *sws_flags = "flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp";
  const char *filtergraph =
    "scale=in_range=full:in_color_matrix=bt709:out_range=tv:out_color_matrix=bt709";

  struct
  {
    // The basic difference is the bitrates... TODO: expand!
    // 0 - proxy
    // 1 - lt
    // 2 - standard
    // 3 - hq
    // 4 - 4444
    // 5 - 4444xq
    const char *profile = "hq";

    // Supported pixel formats (depending on profile)
    //   "yuv422p10le"
    //   "yuv444p10le"
    //   "yuva444p10le" (with alpha)
    const char *pix_fmt = "yuv422p10le";

    // Values between 9 - 13 give a good results.
    int qscale = 11;

    // Tricks QuickTime and Final Cut Pro into thinking that the movie was generated using a
    // QuickTime ProRes encoder.
    const char *vendor = "apl0";
  } pro_res = {};

  // Docs: https://trac.ffmpeg.org/wiki/Encode/H.264
  struct
  {
    // Typically not used according to docs, leave as null to use default.
    // Supported profiles:
    //   "baseline"
    //   "main"
    //   "high"
    //   "high10"  (first 10-bit compatible profile)
    //   "high422" (supports yuv420p, yuv422p, yuv420p10le and yuv422p10le)
    //   "high444" (supports as above as well as yuv444p and yuv444p10le)
    const char *profile = "high";

    // Supported pixel formats (depending on profile)
    //   "yuv420p"
    //   "yuv422p"
    //   "yuv420p10"
    //   "yuv422p10"
    //   "yuv444p"
    //   "yuv444p10"
    const char *pix_fmt = "yuv420p";

    // The available presets in descending order of speed are:
    //   "ultrafast"
    //   "superfast"
    //   "veryfast"
    //   "faster"
    //   "fast"
    //   "medium"
    //   "slow"
    //   "slower"
    //   "veryslow"
    //   ("placebo" - don't use)
    const char *preset = "medium";

    // Current tuning options:
    //   "film"        - use for high quality movie content; lowers deblocking.
    //   "animation"   - good for cartoons; uses higher deblocking and more reference frames.
    //   "grain"       - preserves the grain structure in old, grainy film material.
    //   "stillimage"  - good for slideshow-like content.
    //   "fastdecode"  - allows faster decoding by disabling certain filters.
    //   "zerolatency" - good for fast encoding and low-latency streaming.
    //   ("psnr" - ignore this as it is only used for codec development.)
    //   ("ssim" - ignore this as it is only used for codec development.)
    //
    // If null, no tuning is used.
    const char *tune = nullptr;

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

    const char *x264_params = "keyint=15:no-deblock=1";

    int qp = -1;
#if 0
    // In Nuke:
    // bool fast_start?
    // bool write_timecode?

    int gop_size = 12;
    int b_frames = 0;
    int64_t bit_rate = 400000;
    int bit_rate_tolerance = 0;
    int quantizer_min = 1;
    int quantizer_max = 3;
#endif
  } h264 = {};

#if 0
  // const char* codec_name;
  // const char* pix_fmt;
  // const char* profile;
  // asdff

  MuxVideoCodec video_codec = MuxVideoCodec::kNone;
  MuxPixelFormat pix_fmt = MuxPixelFormat::kNone;

  int64_t bit_rate = 400000;
  int frame_rate = 25;

  // The number of pictures in a group of pictures. Emit one intra frame every N frames at most.
  int gop_size = 12;
#endif

  // Private implementation specific data stored as an opaque pointer.
  // The implementation specific data contains low-level resources such as
  // format context, codecs, etc.
  MuxImpl *impl = nullptr;
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
};

// Initialize the internal resources required to start encoding video frames.
//
// If true is returned, it is possible to start sending frames to the muxer using the
// MuxWriteFrame{} function. At some later point when all frames have been written the internal
// resources must be free'd by calling MuxFree{}.
//
// If false is returned, the internal resources could not be allocated. The mux context will be
// unusable. There is no need to call MuxFree{} in this case.
[[nodiscard]] ILP_MOVIE_EXPORT auto MuxInit(MuxContext *mux_ctx) -> bool;

// Send a frame to the MuxContext encoder. The pixel data interface is determined by the MuxFrame
// struct.
//
// Successful calls return WriteStatus::kEncodingOngoing, errors are reported by returning
// WriteStatus::kEncodingError.
//
// Note that the frame's width/height must match the corresponding values on the MuxContext since
// we currently don't support pixel scaling.
[[nodiscard]] ILP_MOVIE_EXPORT auto MuxWriteFrame(const MuxContext &mux_ctx, const MuxFrame &frame)
  -> bool;

// Finalize the file by flushing streams and possibly performing other operations.
//
// Returns true if successful, otherwise false.
// Note that the MuxContext must be manually free'd using MuxFreeImpl{} regardless of success.
[[nodiscard]] ILP_MOVIE_EXPORT auto MuxFinish(const MuxContext &mux_ctx) -> bool;

// Free the resources allocated by the implementation in MuxInit{}. The implementation pointer will
// be set to null.
//
// MUST be called _before_ MuxContext is destroyed.
//
// Note: Only MuxContext instances that have been passed to a successful MuxInit{} call need to free
// their implementations.
ILP_MOVIE_EXPORT void MuxFree(MuxContext *mux_ctx);

}// namespace ilp_movie
