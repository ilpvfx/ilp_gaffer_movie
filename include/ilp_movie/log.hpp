#pragma once

#include <functional>// std::function

#include <ilp_movie/ilp_movie_export.hpp>

namespace ilp_movie {

namespace LogLevel {
  // Print no output.
  constexpr int kQuiet = -8;

  // Something went really wrong, crashing now.
  constexpr int kPanic = 0;

  // Something went wrong and recovery is not possible.
  constexpr int kFatal = 8;

  // Something went wrong and cannot losslessly be recovered.
  // However, not all future data is affected.
  constexpr int kError = 16;

  // Something somehow does not look correct. This may or may not
  // lead to problems.
  constexpr int kWarning = 24;

  // Standard information.
  constexpr int kInfo = 32;

  // Detailed information.
  constexpr int kVerbose = 40;

  // Stuff which is only useful for developers.
  constexpr int kDebug = 48;

  // Extremely verbose debugging, useful for development.
  constexpr int kTrace = 56;
}// namespace LogLevel

// Set the implementation (libav) log level.
ILP_MOVIE_EXPORT
void SetLogLevel(int level) noexcept;

ILP_MOVIE_EXPORT
void SetLogCallback(const std::function<void(int, const char *)> &cb) noexcept;

// Functions below are for internal use.

ILP_MOVIE_NO_EXPORT
void LogMsg(int level, const char *msg) noexcept;

ILP_MOVIE_NO_EXPORT
void LogInfo(const char *msg) noexcept;

ILP_MOVIE_NO_EXPORT
void LogError(const char *msg) noexcept;

// NOTE: errnum is a libav error code (<0) returned by some function.
// Example:
//
//   if (const int ret = avcodec_parameters_from_context(...); ret < 0) {
//     ilp_movie::LogAvError("Encoder parameters error", ret);
//     return false;
//   }
//
ILP_MOVIE_NO_EXPORT
void LogAvError(const char *msg, const int errnum = 0) noexcept;

}// namespace ilp_movie
