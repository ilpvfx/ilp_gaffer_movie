#include <ilp_movie/log.hpp>

#include <array>// std::array
#include <iostream>// std::cerr
#include <mutex>// std::mutex, std::scoped_lock
#include <sstream>// std::ostringstream

// clang-format off
extern "C" {
#include <libavutil/log.h>
#include <libavutil/error.h>
}
// clang-format on

static std::mutex mutex;

static int IlpLogLevel = ilp_movie::LogLevel::kInfo;

// clang-format off
static std::function<void(int, const char *)> IlpLogCallback = 
  [](int /*level*/, const char * /*s*/) noexcept {};
// clang-format on

[[nodiscard]] static constexpr auto GetIlpLogLevel(const int av_level) noexcept -> int
{
  // clang-format off
  auto level = ilp_movie::LogLevel::kInfo;
  switch (av_level) {
  case AV_LOG_QUIET:   level = ilp_movie::LogLevel::kQuiet;   break;
  case AV_LOG_PANIC:   level = ilp_movie::LogLevel::kPanic;   break;
  case AV_LOG_FATAL:   level = ilp_movie::LogLevel::kFatal;   break;
  case AV_LOG_ERROR:   level = ilp_movie::LogLevel::kError;   break;
  case AV_LOG_WARNING: level = ilp_movie::LogLevel::kWarning; break;
  case AV_LOG_INFO:    level = ilp_movie::LogLevel::kInfo;    break;
  case AV_LOG_VERBOSE: level = ilp_movie::LogLevel::kVerbose; break;
  case AV_LOG_DEBUG:   level = ilp_movie::LogLevel::kDebug;   break;
  case AV_LOG_TRACE:   level = ilp_movie::LogLevel::kTrace;   break;
  default:                                                    break;
  }
  // clang-format on
  return level;
}

[[nodiscard]] static constexpr auto LogLevelString(const int level) noexcept -> const char *
{
  // clang-format off
  switch (level) {
  case ilp_movie::LogLevel::kQuiet:   return "[QUIET]"; // ???
  case ilp_movie::LogLevel::kPanic:   return "[PANIC]";
  case ilp_movie::LogLevel::kFatal:   return "[FATAL]";
  case ilp_movie::LogLevel::kError:   return "[ERROR]";
  case ilp_movie::LogLevel::kWarning: return "[WARNING]";
  case ilp_movie::LogLevel::kInfo:    return "[INFO]";
  case ilp_movie::LogLevel::kVerbose: return "[VERBOSE]";
  case ilp_movie::LogLevel::kDebug:   return "[DEBUG]";
  case ilp_movie::LogLevel::kTrace:   return "[TRACE]";
  default:                            return "[UNKNOWN]";
  }
  // clang-format on
}

[[nodiscard]] static auto BuildMessage(const int level, const char *msg) noexcept -> std::string
{
  std::string s;
  try {
    // NOTE(tohi): Not adding newline character!
    std::ostringstream oss;
    oss << "[ilp_movie]" << LogLevelString(ilp_movie::LogLevel::kError) << msg;
    s = oss.str();
  } catch (...) {
    // Swallow exception, returned string should be empty.
  }
  return s;
}

// Hook into the av_log calls so that we can pass a string to a user-provided logger.
// Needs to be thread-safe!
static void IlpMovieAvLogCallback(void *ptr, int level, const char *fmt, va_list vl) noexcept
{
  static constexpr int kLineSize = 1024;
  static std::array<char, kLineSize> line = {};
  static int print_prefix = 1;

  try {
    // Filter messages based on log level. We can do this before
    // locking the mutex.
    const auto ilp_level = GetIlpLogLevel(level);
    if (ilp_level > IlpLogLevel) { return; }

    std::scoped_lock lock{ mutex };
    const int ret = av_log_format_line2(ptr, level, fmt, vl, line.data(), kLineSize, &print_prefix);

    // Don't call LogMsg below, since we have already locked the mutex above.
    if (ret < 0) {
      IlpLogCallback(ilp_movie::LogLevel::kError,
        BuildMessage(ilp_movie::LogLevel::kError, "Logging error\n").c_str());
      return;
    } else if (!(ret < kLineSize)) {
      IlpLogCallback(ilp_movie::LogLevel::kError,
        BuildMessage(ilp_movie::LogLevel::kError, "Truncated log message\n").c_str());
    }

    // Send raw message from libav to callback, don't add any decorations.
    IlpLogCallback(ilp_level, line.data());
  } catch (...) {
    std::cerr << "unknown exception\n";
  }
}

namespace {
struct AvLogCallbackInstaller
{
  AvLogCallbackInstaller() noexcept { av_log_set_callback(IlpMovieAvLogCallback); }
};
}// namespace
static AvLogCallbackInstaller _dummy;

namespace ilp_movie {

void SetLogLevel(const int level) noexcept { IlpLogLevel = level; }

void SetLogCallback(const std::function<void(int, const char *)> &cb) noexcept
{
  IlpLogCallback = cb;
}

void LogMsg(const int level, const char *msg) noexcept
{
  if (level > IlpLogLevel) { return; }

  std::string s;
  try {
    // Filter messages based on log level.

    s = BuildMessage(level, msg);

    std::scoped_lock lock{ mutex };
    IlpLogCallback(level, s.c_str());
  } catch (...) {
    std::cerr << "unknown exception\n";
  }
}

void LogInfo(const char *msg) noexcept { LogMsg(LogLevel::kInfo, msg); }

void LogError(const char *msg) noexcept { LogMsg(LogLevel::kError, msg); }

void LogAvError(const char *msg, const int errnum) noexcept
{
  std::string s;
  try {
    std::ostringstream oss;
    oss << msg;
    if (errnum < 0) {
      std::array<char, AV_ERROR_MAX_STRING_SIZE> errbuf = {};
      av_strerror(errnum, errbuf.data(), AV_ERROR_MAX_STRING_SIZE);
      oss << ": " << errbuf.data();
    }
    oss << '\n';
    s = oss.str();
    LogMsg(LogLevel::kError, s.c_str());
  } catch (...) {
    std::cerr << "unknown exception\n";
  }
}

}// namespace ilp_movie
