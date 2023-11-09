#pragma once

#include <cstddef>// std::size_t
#include <memory>// std::unique_ptr
#include <string>// std::string
#include <vector>// std::vector

#include <ilp_movie/ilp_movie_export.hpp>// ILP_MOVIE_EXPORT
#include <ilp_movie/pixel_format.hpp>// ilp_movie::PixelFormat

namespace ilp_movie {

struct DecodeVideoStreamInfo
{
  int stream_index = -1;
  bool is_best = false;

  int width = -1;
  int height = -1;
  struct
  {
    int num = 0;
    int den = 0;
  } frame_rate;

  // One-based frame number range. Seek should return valid frames for the inclusive range:
  // [first, first + count - 1].
  int64_t first_frame_nb = -1;
  int64_t frame_count = 0;

  // PixelFormat pix_fmt = PixelFormat::kNone;

  const char *pix_fmt_name;
  const char *color_range_name;
  const char *color_space_name;
  const char *color_primaries_name;
};

struct DecodedVideoFrame
{
  int width = -1;
  int height = -1;
  int64_t frame_nb = -1;
  bool key_frame = false;
  double pixel_aspect_ratio = -1.0;

  PixelFormat pix_fmt = PixelFormat::kNone;

  // NOTE(tohi): If we wanted to support non-floating point formats as well this would need to be
  //             a byte buffer: std::unique_ptr<std::uint8_t> (or std::byte).
  struct
  {
    std::unique_ptr<float[]> data = nullptr;
    std::size_t count = 0;
  } buf = {};
};

class DecoderImpl;
class ILP_MOVIE_EXPORT Decoder
{
public:
  explicit Decoder(std::string path);
  ~Decoder();

  // Movable.
  Decoder(Decoder &&rhs) noexcept = default;
  Decoder &operator=(Decoder &&rhs) noexcept = default;

  // Not copyable.
  Decoder(const Decoder &rhs) = delete;
  Decoder &operator=(const Decoder &rhs) = delete;

  [[nodiscard]] auto StreamInfo() const noexcept -> std::vector<DecodeVideoStreamInfo>;
  
  [[nodiscard]] auto SetStreamFilterGraph(int stream_index,
    std::string filter_descr = "null") noexcept -> bool;

  [[nodiscard]] auto
    DecodeVideoFrame(int stream_index, int frame_nb, DecodedVideoFrame &dvf) noexcept -> bool;

private:
  const DecoderImpl *Pimpl() const { return _pimpl.get(); }
  DecoderImpl *Pimpl() { return _pimpl.get(); }

  std::unique_ptr<DecoderImpl> _pimpl;
};

}// namespace ilp_movie
