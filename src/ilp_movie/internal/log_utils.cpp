#include <internal/log_utils.hpp>

#include <array>// std::array
#include <iostream>// std::cerr
#include <sstream>// std::ostringstream
#include <string>// std::string

// clang-format off
extern "C" {
#include <libavutil/error.h>// AV_ERROR_MAX_STRING_SIZE, av_str_error
#include <libavutil/rational.h>// AVRational
#include <libavutil/timestamp.h>// av_ts2timestr
#include <libavformat/avformat.h>// AVFormatContext
#include <libavcodec/packet.h>// AVPacket
}
// clang-format on

#include <ilp_movie/log.hpp>// ilp_movie::LogMsg, ilp_movie::LogLevel

namespace log_utils_internal {

void LogAvError(const char *const msg, const int errnum) noexcept
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

void LogPacket(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const int log_level) noexcept
{
  const auto ts_to_str = [](const int64_t ts) {
    std::array<char, AV_TS_MAX_STRING_SIZE> buf{};// Initialize to zero.
    av_ts_make_string(buf.data(), ts);
    return std::string{ buf.data() };
  };

  const auto ts_to_timestr = [](const int64_t ts, AVRational *const tb) {
    std::array<char, AV_TS_MAX_STRING_SIZE> buf{};// Initialize to zero.
    av_ts_make_time_string(buf.data(), ts, tb);
    return std::string{ buf.data() };
  };

  AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;// NOLINT
  // clang-format off
  std::ostringstream oss;
  oss << "pts:" << ts_to_str(pkt->pts) << " pts_time:" << ts_to_timestr(pkt->pts, time_base) 
      << " dts:" << ts_to_str(pkt->dts) << " dts_time:" << ts_to_timestr(pkt->dts, time_base) 
      << " duration:" << ts_to_str(pkt->duration) << " duration_time:" << ts_to_timestr(pkt->duration, time_base) 
      << " stream_index:" << pkt->stream_index << "\n";

  ilp_movie::LogMsg(log_level, oss.str().c_str());
  // clang-format on
}

}// namespace log_utils_internal
