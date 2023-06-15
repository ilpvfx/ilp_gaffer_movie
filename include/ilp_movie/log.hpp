#pragma once

#include <functional>// std::function

#include <ilp_mux/ilp_mux_export.hpp>

namespace ilp_movie {

enum class LogLevel : int {
  // Print no output.
  kQuiet = 0,

  // Something went really wrong, crashing now.
  kPanic,

  // Something went wrong and recovery is not possible.
  kFatal,

  // Something went wrong and cannot losslessly be recovered.
  // However, not all future data is affected.
  kError,

  // Something somehow does not look correct. This may or may not
  // lead to problems.
  kWarning,

  // Standard information.
  kInfo,

  // Detailed information.
  kVerbose,

  // Stuff which is only useful for developers.
  kDebug,

  // Extremely verbose debugging, useful for development.
  kTrace,
};

ILP_MUX_EXPORT
void SetLogLevel(LogLevel level);

ILP_MUX_EXPORT
void SetLogCallback(const std::function<void(const char *)> &cb);

void LogError(const char *msg, const int errnum = 0);
void LogInfo(const char *msg);

void InstallLogCallback();

}// namespace ilp_movie
