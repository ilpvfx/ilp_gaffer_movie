#include <internal/timestamp.hpp>

#include <cassert>// assert
#include <cmath>// std::round

// clang-format off
extern "C" {
#include <libavutil/avutil.h>// AV_NOPTS_VALUE
#include <libavutil/rational.h>// av_q2d, av_mul_q, etc.
}
// clang-format on

namespace timestamp_internal {

[[nodiscard]] auto FrameToPts(const int frame_index,
  const int frame_rate_num,
  const int frame_rate_den,
  const int tb_num,
  const int tb_den,
  const int64_t start_pts) noexcept -> int64_t
{
  const int64_t num = static_cast<int64_t>(frame_index) * frame_rate_den * tb_den;
  const int64_t den = static_cast<int64_t>(frame_rate_num) * tb_num;
  return start_pts + den > 0 ? (num / den) : num;
}

[[nodiscard]] auto PtsToFrame(int64_t pts,
  int frame_rate_num,
  int frame_rate_den,
  int tb_num,
  int tb_den,
  int64_t start_pts) noexcept -> int
{
  const int64_t num = (pts - start_pts) * tb_num * frame_rate_num;
  const int64_t den = int64_t(tb_den) * frame_rate_den;
  return static_cast<int>(den > 0 ? (num / den) : num);
}

#if 0
auto FrameIndexToTimestamp(const std::int64_t frame_index,
  const AVRational &frame_rate,
  const AVRational &time_base,
  const std::int64_t start_time) noexcept -> std::int64_t
{
  assert(!(frame_rate.num == 0 && frame_rate.den == 1));// NOLINT
  assert(!(time_base.num == 0 && time_base.den == 1));// NOLINT
  assert(start_time != AV_NOPTS_VALUE);// NOLINT

  return static_cast<std::int64_t>(
           std::round(static_cast<double>(frame_index)
                      * av_q2d(av_mul_q(av_inv_q(frame_rate), av_inv_q(time_base)))))
         - start_time;
}

auto TimestampToFrameIndex(const std::int64_t timestamp,
  const AVRational &frame_rate,
  const AVRational &time_base,
  const std::int64_t start_time) noexcept -> std::int64_t
{
  assert(!(frame_rate.num == 0 && frame_rate.den == 1));// NOLINT
  assert(!(time_base.num == 0 && time_base.den == 1));// NOLINT
  assert(start_time != AV_NOPTS_VALUE);// NOLINT

  // AVRational q = av_mul_q(frame_rate, time_base);
  // q.num *= (timestamp + start_time);
  // return static_cast<int64_t>(std::round(av_q2d(q)));

  return static_cast<std::int64_t>(std::round(
    static_cast<double>(timestamp + start_time) * av_q2d(av_mul_q(frame_rate, time_base))));
}
#endif

}// namespace timestamp_internal
