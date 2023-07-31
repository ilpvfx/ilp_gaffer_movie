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

}// namespace timestamp_internal