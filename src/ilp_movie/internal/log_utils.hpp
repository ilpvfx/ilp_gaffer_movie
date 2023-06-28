#pragma once

#include <ilp_movie/ilp_movie_export.hpp>// ILP_MOVIE_NO_EXPORT
#include <ilp_movie/log.hpp>// ilp_movie::LogMsg, ilp_movie::LogLevel

struct AVFormatContext;
struct AVPacket;

namespace log_utils_internal {

// NOTE: errnum is a libav error code (<0) returned by some function.
// Example:
//
//   if (const int ret = avcodec_parameters_from_context(...); ret < 0) {
//     ilp_movie::log_utils::LogAvError("Encoder parameters error", ret);
//     return false;
//   }
//
// Typically, the message string will not end with a newline character since
// one is automatically added to the final message that is built internally.
ILP_MOVIE_NO_EXPORT
void LogAvError(const char *msg, const int errnum = 0) noexcept;


ILP_MOVIE_NO_EXPORT void LogPacket(const AVFormatContext *fmt_ctx,
  const AVPacket *pkt,
  const int log_level = ilp_movie::LogLevel::kTrace) noexcept;

}// namespace log_utils_internal
