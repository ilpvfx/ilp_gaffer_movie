#include <internal/log_utils.hpp>

#include <array>// std::array
#include <iostream>// std::cerr
#include <sstream>// std::ostringstream

// clang-format off
extern "C" {
#include <libavutil/error.h>// AV_ERROR_MAX_STRING_SIZE, av_str_error
}
// clang-format on

#include <ilp_movie/log.hpp>// ilp_movie::LogMsg, ilp_movie::LogLevel

namespace log_utils_internal {

void LogAvError(const char *msg, const int errnum) noexcept
{
  std::ostringstream oss;
  oss << msg;
  if (errnum < 0) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> errbuf = {};
    av_strerror(errnum, errbuf.data(), AV_ERROR_MAX_STRING_SIZE);
    oss << ": " << errbuf.data();
  }
  oss << '\n';
  ilp_movie::LogMsg(ilp_movie::LogLevel::kError, oss.str().c_str());
}

}// namespace log_utils_internal
