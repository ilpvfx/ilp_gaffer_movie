// Rationale:
//
// f = t * (fr.num / fr.den)   <=>   t = f * (fr.den / fr.num)
//
// where f is the (integer) frame index, t is time in seconds, and fr is the frame rate (expressed
// as frames / second, e.g. [24 / 1]). Now we want to convert the time, t, into a timestamp
// (integer), ts, in the provided time base (tb):
//
// t = (ts + ts_0) * (tb.num / tb.den)   <=>   ts = t * (tb.den / tb.num) - ts_0
//
// Substituting in our expression for t from above:
//
// ts = f * (fr.den * tb.den / fr.num * tb.num) - ts_0
//
// NOTE(tohi) that we assume a constant frame rate here!

#pragma once

#include <cstdint>// std::int64_t

struct AVRational;

namespace timestamp_internal {

[[nodiscard]] auto FrameIndexToTimestamp(const std::int64_t frame_index,
  const AVRational &frame_rate,
  const AVRational &time_base,
  const std::int64_t start_time) noexcept -> std::int64_t;

[[nodiscard]] auto TimestampToFrameIndex(const std::int64_t timestamp,
  const AVRational &frame_rate,
  const AVRational &time_base,
  const std::int64_t start_time) noexcept -> std::int64_t;

}// namespace timestamp_internal
