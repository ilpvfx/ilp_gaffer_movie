#include <ilp_movie/log.hpp>

#include <array>// std::array
#include <mutex>// std::mutex, std::scoped_lock
#include <sstream>// std::ostringstream

// clang-format off
extern "C" {
#include <libavutil/log.h>// av_log_set_callback, av_log_format_line2
}
// clang-format on

static std::mutex mutex;

// clang-format off
static std::function<void(int, const char *)> IlpLogCallback = 
  [](int /*level*/, const char * /*s*/) noexcept {};
// clang-format on

[[nodiscard]] static constexpr auto GetIlpLogLevel(const int av_level) noexcept -> int
{
  // clang-format off
  auto ilp_level = ilp_movie::LogLevel::kInfo;
  switch (av_level) {
  case AV_LOG_QUIET:   ilp_level = ilp_movie::LogLevel::kQuiet;   break;
  case AV_LOG_PANIC:   ilp_level = ilp_movie::LogLevel::kPanic;   break;
  case AV_LOG_FATAL:   ilp_level = ilp_movie::LogLevel::kFatal;   break;
  case AV_LOG_ERROR:   ilp_level = ilp_movie::LogLevel::kError;   break;
  case AV_LOG_WARNING: ilp_level = ilp_movie::LogLevel::kWarning; break;
  case AV_LOG_INFO:    ilp_level = ilp_movie::LogLevel::kInfo;    break;
  case AV_LOG_VERBOSE: ilp_level = ilp_movie::LogLevel::kVerbose; break;
  case AV_LOG_DEBUG:   ilp_level = ilp_movie::LogLevel::kDebug;   break;
  case AV_LOG_TRACE:   ilp_level = ilp_movie::LogLevel::kTrace;   break;
  default:                                                        break;
  }
  // clang-format on
  return ilp_level;
}

[[nodiscard]] static constexpr auto GetAvLogLevel(const int ilp_level) noexcept -> int
{
  // clang-format off
  auto av_level = ilp_movie::LogLevel::kInfo;
  switch (ilp_level) {
  case AV_LOG_QUIET:   av_level = ilp_movie::LogLevel::kQuiet;   break;
  case AV_LOG_PANIC:   av_level = ilp_movie::LogLevel::kPanic;   break;
  case AV_LOG_FATAL:   av_level = ilp_movie::LogLevel::kFatal;   break;
  case AV_LOG_ERROR:   av_level = ilp_movie::LogLevel::kError;   break;
  case AV_LOG_WARNING: av_level = ilp_movie::LogLevel::kWarning; break;
  case AV_LOG_INFO:    av_level = ilp_movie::LogLevel::kInfo;    break;
  case AV_LOG_VERBOSE: av_level = ilp_movie::LogLevel::kVerbose; break;
  case AV_LOG_DEBUG:   av_level = ilp_movie::LogLevel::kDebug;   break;
  case AV_LOG_TRACE:   av_level = ilp_movie::LogLevel::kTrace;   break;
  default:                                                       break;
  }
  // clang-format on
  return av_level;
}

// Hook into the av_log calls so that we can pass a string to a user-provided logger.
// Needs to be thread-safe!
static void IlpMovieAvLogCallback(void *ptr, int av_level, const char *fmt, va_list vl) noexcept
{
  /*static*/ constexpr int kLineSize = 1024;
  /*static*/ std::array<char, kLineSize> line = {};
  /*static*/ int print_prefix = 1;

  // Filter messages based on log level. We can do this before locking the mutex.
  if (av_level > av_log_get_level()) { return; }

  std::scoped_lock lock{ mutex };
  const int ret =
    av_log_format_line2(ptr, av_level, fmt, vl, line.data(), kLineSize, &print_prefix);

  // Don't call LogMsg below, since we have already locked the mutex above.
  if (ret < 0) {
    IlpLogCallback(ilp_movie::LogLevel::kError, "Error in av_log_format_line2");
    return;
  } else if (!(ret < kLineSize)) {
    IlpLogCallback(ilp_movie::LogLevel::kError, "Truncated log message");
    return;
  }

  // Send raw message from libav to callback, don't add any decorations.
  IlpLogCallback(GetIlpLogLevel(av_level), line.data());
}

namespace {
struct AvLogCallbackInstaller
{
  AvLogCallbackInstaller() noexcept
  {
    int flags = 0;
    flags |= AV_LOG_SKIP_REPEATED;// NOLINT
    // flags |= AV_LOG_PRINT_LEVEL;
    av_log_set_flags(flags);// NOLINT
    av_log_set_callback(IlpMovieAvLogCallback);
  }
};
}// namespace
static AvLogCallbackInstaller _dummy;

namespace ilp_movie {

void SetLogLevel(const int level) noexcept
{
  av_log_set_level(GetAvLogLevel(level));
}

auto GetLogLevel() noexcept -> int { return GetIlpLogLevel(av_log_get_level()); }

void SetLogCallback(const std::function<void(int, const char *)> &cb) noexcept
{
  IlpLogCallback = cb;
}

auto GetLogCallback() noexcept -> std::function<void(int, const char *)> {
  return IlpLogCallback;
}

auto LogLevelString(const int level) noexcept -> const char *
{
  // clang-format off
  switch (level) {
  case ilp_movie::LogLevel::kQuiet:   return "QUIET"; // Hrm...
  case ilp_movie::LogLevel::kPanic:   return "PANIC";
  case ilp_movie::LogLevel::kFatal:   return "FATAL";
  case ilp_movie::LogLevel::kError:   return "ERROR";
  case ilp_movie::LogLevel::kWarning: return "WARNING";
  case ilp_movie::LogLevel::kInfo:    return "INFO";
  case ilp_movie::LogLevel::kVerbose: return "VERBOSE";
  case ilp_movie::LogLevel::kDebug:   return "DEBUG";
  case ilp_movie::LogLevel::kTrace:   return "TRACE";
  default:                            return "UNKNOWN";
  }
  // clang-format on
}

void LogMsg(const int level, const char *msg) noexcept
{
  // Filter messages based on log level, we can do this before locking the mutex.
  if (level > GetLogLevel()) { return; }
  std::scoped_lock lock{ mutex };
  IlpLogCallback(level, msg);
}

}// namespace ilp_movie
