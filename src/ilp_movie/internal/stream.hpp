#pragma once

#include <cstdint>// int64_t, etc
#include <memory>// std::unique_ptr

// extern "C" {
// #include <libavutil/avutil.h>// AVMediaType
// }// "C"

#include <internal/frame_rate.hpp>

struct AVCodecContext;
struct AVFormatContext;
struct AVPacket;
struct AVStream;

namespace ilp_movie {
namespace stream_internal {

  namespace StreamType {
    // clang-format off
    using ValueType = uint8_t;
    constexpr ValueType kUnknown  = 0U;
    constexpr ValueType kVideo    = 1U << 1U;
    constexpr ValueType kAudio    = 1U << 2U;
    constexpr ValueType kTimecode = 1U << 3U;
    constexpr ValueType kAll      = kVideo | kAudio | kTimecode;
    // clang-format on
  }// namespace StreamType

  class StreamImpl;
  class Stream
  {
  public:
    Stream(AVFormatContext *av_format_ctx, AVStream *av_stream, int index, int thread_count = 0);
    ~Stream();

    // Movable.
    Stream(Stream &&rhs) noexcept = default;
    Stream &operator=(Stream &&rhs) noexcept = default;

    // Not copyable.
    Stream(const Stream &rhs) = delete;
    Stream &operator=(const Stream &rhs) = delete;

    [[nodiscard]] auto StreamIndex() const noexcept -> int;

    [[nodiscard]] auto StartPts() const noexcept -> int64_t;
    [[nodiscard]] auto FrameToPts(int frame) const noexcept -> int64_t;
    [[nodiscard]] auto SecondsToPts(double secs) const noexcept -> int64_t;
    [[nodiscard]] auto DurationSeconds() const noexcept -> double;
    [[nodiscard]] auto DurationFrames() const noexcept -> int;


    [[nodiscard]] auto RawStream() const noexcept -> AVStream*;

    [[nodiscard]] auto CodecContext() const noexcept -> AVCodecContext*;

    [[nodiscard]] auto SendPacket(AVPacket *av_packet) noexcept -> bool;
    [[nodiscard]] auto SendFlushPacket() noexcept -> bool;


    void FlushBuffers() noexcept;

    void ReceiveFrame();

  private:
    const StreamImpl *Pimpl() const { return _pimpl.get(); }
    StreamImpl *Pimpl() { return _pimpl.get(); }

    std::unique_ptr<StreamImpl> _pimpl;
  };

}// namespace stream_internal
}// namespace ilp_movie