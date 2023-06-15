#include <ilp_movie/log.hpp>

#include <array>// std::array
#include <mutex>//std::mutex, std::scoped_lock
#include <sstream>// std::ostringstream

// clang-format off
extern "C" {
#include <libavutil/log.h>
#include <libavutil/error.h>
}
// clang-format on

static std::mutex mutex;
static std::function<void(const char *)> IlpVideoLog = [](const char * /*s*/) {};

// Hook into the av_log calls so that we can pass a string to a user-provided logger.
// Needs to be thread-safe!
static void IlpMovieLogCallback(void *ptr, int level, const char *fmt, va_list vl)
{
  static constexpr int kLineSize = 1024;
  std::array<char, kLineSize> line = {};
  static int print_prefix = 1;

  std::scoped_lock lock{ mutex };

  const int ret = av_log_format_line2(ptr, level, fmt, vl, line.data(), kLineSize, &print_prefix);
  if (ret < 0) {
    IlpVideoLog("[ilp_movie] Logging error\n");
    return;
  } else if (!(ret < kLineSize)) {
    IlpVideoLog("[ilp_movie] Truncated log message\n");
  }
  IlpVideoLog(line.data());
}

namespace ilp_movie {

void SetLogLevel(const LogLevel level)
{
  // clang-format off
  switch (level) {
  case LogLevel::kQuiet:   av_log_set_level(AV_LOG_QUIET);   break;
  case LogLevel::kPanic:   av_log_set_level(AV_LOG_PANIC);   break;
  case LogLevel::kFatal:   av_log_set_level(AV_LOG_FATAL);   break;
  case LogLevel::kError:   av_log_set_level(AV_LOG_ERROR);   break;
  case LogLevel::kWarning: av_log_set_level(AV_LOG_WARNING); break;
  case LogLevel::kInfo:    av_log_set_level(AV_LOG_INFO);    break;
  case LogLevel::kVerbose: av_log_set_level(AV_LOG_VERBOSE); break;
  case LogLevel::kDebug:   av_log_set_level(AV_LOG_DEBUG);   break;
  case LogLevel::kTrace:   av_log_set_level(AV_LOG_TRACE);   break;
  default: /* Do nothing, keep current log level. */         break; 
  }
  // clang-format on
}

void SetLogCallback(const std::function<void(const char *)> &cb) { IlpVideoLog = cb; }

void LogError(const char *msg, const int errnum)
{
  std::ostringstream oss;
  oss << "[ilp_movie] " << msg;
  if (errnum < 0) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> errbuf = {};
    av_strerror(errnum, errbuf.data(), AV_ERROR_MAX_STRING_SIZE);
    oss << ": " << errbuf.data() << '\n';
  } else {
    oss << '\n';
  }
  IlpVideoLog(oss.str().c_str());
}

void LogInfo(const char *msg)
{
  std::ostringstream oss;
  oss << "[ilp_movie] " << msg << '\n';
  IlpVideoLog(oss.str().c_str());
}

void InstallLogCallback() { av_log_set_callback(IlpMovieLogCallback); }

}// namespace ilp_movie
