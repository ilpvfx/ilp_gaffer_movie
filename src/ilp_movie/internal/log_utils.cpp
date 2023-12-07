#include <internal/log_utils.hpp>

#include <array>// std::array
#include <iostream>// std::cerr
#include <sstream>// std::ostringstream
#include <string>// std::string

// clang-format off
extern "C" {
#include <libavutil/error.h>// AV_ERROR_MAX_STRING_SIZE, av_str_error
#include <libavutil/rational.h>// AVRational
#include <libavutil/timestamp.h>// AV_TS_MAX_STRING_SIZE, av_ts2timestr
#include <libavformat/avformat.h>// AVFormatContext
#include <libavcodec/packet.h>// AVPacket
}
// clang-format on

#include <ilp_movie/log.hpp>// ilp_movie::LogMsg, ilp_movie::LogLevel

namespace log_utils_internal {

void LogAvError(const char *const msg, const int errnum, const int log_level) noexcept
{
  std::ostringstream oss;
  oss << msg;
  if (errnum < 0) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> errbuf = {};
    av_strerror(errnum, errbuf.data(), AV_ERROR_MAX_STRING_SIZE);
    oss << ": " << errbuf.data();
  }
  oss << '\n';
  ilp_movie::LogMsg(log_level, oss.str().c_str());
}

void LogPacket(const AVFormatContext *const fmt_ctx,
  const AVPacket *const pkt,
  const int log_level) noexcept
{
  std::array<char, AV_TS_MAX_STRING_SIZE> buf{};// Initialize to zero.
  AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;// NOLINT

  const auto ts_to_str = [&](const int64_t ts) {
    return av_ts_make_string(buf.data(), ts);
  };

  const auto ts_to_timestr = [&](const int64_t ts) {
    return av_ts_make_time_string(buf.data(), ts, time_base);
  };

  // clang-format off
  std::ostringstream oss;
  oss << "pts:" << ts_to_str(pkt->pts) << " pts_time:" << ts_to_timestr(pkt->pts) 
      << " dts:" << ts_to_str(pkt->dts) << " dts_time:" << ts_to_timestr(pkt->dts) 
      << " duration:" << ts_to_str(pkt->duration) << " duration_time:" << ts_to_timestr(pkt->duration) 
      << " stream_index:" << pkt->stream_index << "\n";
  ilp_movie::LogMsg(log_level, oss.str().c_str());
  // clang-format on
}

}// namespace log_utils_internal
