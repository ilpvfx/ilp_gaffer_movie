#pragma once

#include <chrono>

#include <internal/external/timebase/flicks.hpp>

namespace ilp_movie {
namespace frame_rate_internal {

  class FrameRate : public timebase::flicks
  {
  public:
    explicit FrameRate(const timebase::flicks &flicks = timebase::k_flicks_zero_seconds) noexcept
      : timebase::flicks(flicks)
    {}
    explicit FrameRate(const double seconds) noexcept : FrameRate(timebase::to_flicks(seconds)) {}

    FrameRate(const FrameRate &) = default;
    FrameRate(FrameRate &&) = default;
    ~FrameRate() = default;

    // double to_double() const;
    [[nodiscard]] auto to_microseconds() const noexcept -> std::chrono::microseconds
    {
      return std::chrono::duration_cast<std::chrono::microseconds>(*this);
    }

    // [[nodiscard]] std::chrono::nanoseconds::rep count() const { return count(); }
    [[nodiscard]] auto to_seconds() const noexcept -> double { return timebase::to_seconds(*this); }
    [[nodiscard]] auto to_fps() const noexcept -> double
    {
      double fps = 0.0;
      if (count()) { fps = 1.0 / timebase::to_seconds(*this); }
      return fps;
    }
    [[nodiscard]] auto to_flicks() const noexcept -> timebase::flicks { return *this; }

    FrameRate &operator=(const FrameRate &) = default;
    FrameRate &operator=(FrameRate &&) = default;

    [[nodiscard]] operator bool() const noexcept { return count() != 0; }
  };

  [[nodiscard]] inline auto to_string(const FrameRate &v) noexcept -> std::string
  {
    return std::to_string(v.to_fps());
  }

}// namespace frame_rate_internal
}// namespace ilp_movie
