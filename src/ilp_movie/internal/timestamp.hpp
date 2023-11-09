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
//
// NOTE: pts * stream.time_base.num / stream.time_base.den = [s]
#pragma once

#include <cstdint>// int64_t

namespace timestamp_internal {

// Frame index is zero-based.
// Assumes start_pts is not AV_NOPTS_VALUE.
[[nodiscard]] auto FrameToPts(int frame_index,
  int frame_rate_num,
  int frame_rate_den,
  int tb_num,
  int tb_den,
  int64_t start_pts) noexcept -> int64_t;

// Returns zero-based frame index.
// Assumes start_pts is not AV_NOPTS_VALUE.
[[nodiscard]] auto PtsToFrame(int64_t pts,
  int frame_rate_num,
  int frame_rate_den,
  int tb_num,
  int tb_den,
  int64_t start_pts) noexcept -> int;

}// namespace timestamp_internal
