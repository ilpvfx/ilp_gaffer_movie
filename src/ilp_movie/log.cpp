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
  // NOTE(tohi): Not adding newline character!
  std::ostringstream oss;
  oss << "[ilp_movie]" << LogLevelString(level) << " " << msg;
  return oss.str();
}

// Hook into the av_log calls so that we can pass a string to a user-provided logger.
// Needs to be thread-safe!
static void IlpMovieAvLogCallback(void *ptr, int level, const char *fmt, va_list vl) noexcept
{
  static constexpr int kLineSize = 1024;
  static std::array<char, kLineSize> line = {};
  static int print_prefix = 1;

  // Filter messages based on log level. We can do this before
  // locking the mutex.
  const auto ilp_level = GetIlpLogLevel(level);
  if (ilp_level > IlpLogLevel) { return; }

  std::scoped_lock lock{ mutex };
  const int ret = av_log_format_line2(ptr, level, fmt, vl, line.data(), kLineSize, &print_prefix);

  // Don't call LogMsg below, since we have already locked the mutex above.
  if (ret < 0) {
    IlpLogCallback(ilp_movie::LogLevel::kError,
      BuildMessage(ilp_movie::LogLevel::kError, "Error in av_log_format_line2\n").c_str());
    return;
  } else if (!(ret < kLineSize)) {
    IlpLogCallback(ilp_movie::LogLevel::kError,
      BuildMessage(ilp_movie::LogLevel::kError, "Truncated log message\n").c_str());
  }

  // Send raw message from libav to callback, don't add any decorations.
  IlpLogCallback(ilp_level, line.data());
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
  // Filter messages based on log level and build the message. We can do this before
  // locking the mutex.
  if (level > IlpLogLevel) { return; }
  const std::string s = BuildMessage(level, msg);

  std::scoped_lock lock{ mutex };
  IlpLogCallback(level, s.c_str());
}

}// namespace ilp_movie
