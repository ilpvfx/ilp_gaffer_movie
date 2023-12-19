#pragma once

#include <cstddef>// std::size_t
#include <cstdint>// int64_t, etc.
#include <memory>// std::unique_ptr
#include <optional>// std::optional
#include <string>// std::string
#include <vector>// std::vector

#include <ilp_movie/ilp_movie_export.hpp>// ILP_MOVIE_EXPORT
#include <ilp_movie/pixel_format.hpp>// ilp_movie::PixelFormat

namespace ilp_movie {

struct InputVideoStreamHeader
{
  int stream_index = -1;
  bool is_best = false;

  // One-based frame number range. Seek should return valid frames for the inclusive range:
  // [first, first + count - 1].
  int64_t first_frame_nb = -1;
  int64_t frame_count = 0;

  int width = -1;
  int height = -1;
  struct
  {
    int num = 0;
    int den = 0;
  } frame_rate;

  // Pixel width / height.
  struct
  {
    int num = 0;
    int den = 0;
  } pixel_aspect_ratio;

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
  struct
  {
    int num = 0;
    int den = 1;
  } pixel_aspect_ratio = {};

  PixelFormat pix_fmt = PixelFormat::kNone;

  // NOTE(tohi): If we wanted to support non-floating point formats as well this would need to be
  //             a byte buffer: std::unique_ptr<std::uint8_t> (or std::byte).
  struct
  {
    std::unique_ptr<float[]> data = nullptr;
    std::size_t count = 0;
  } buf = {};
};

struct DecoderFilterGraphDescription
{
  // Pass-through (for video), use "anull" for audio.
  std::string filter_descr = "null";

  // The pixel format of the decoded frames after they've been pulled from
  // the filter graph. This string will be converted to a suitable enum internally
  // but we don't want to expose those enum types on this interface.
  //
  // E.g. "gbrpf32le" for 32-bit float (planar) RGB frames.
  std::string out_pix_fmt_name = "";
};

class DecoderImpl;
class ILP_MOVIE_EXPORT Decoder
{
public:
  [[nodiscard]] static auto SupportedFormats() -> std::vector<std::string>;

  Decoder();
  ~Decoder();

  // Movable.
  Decoder(Decoder &&rhs) noexcept = default;
  Decoder &operator=(Decoder &&rhs) noexcept = default;

  // Not copyable.
  Decoder(const Decoder &rhs) = delete;
  Decoder &operator=(const Decoder &rhs) = delete;

  [[nodiscard]] auto Open(const std::string &url,
    const DecoderFilterGraphDescription &dfgd) noexcept -> bool;

  // Returns true if the decoder has been successfully opened and has not been closed since.
  [[nodiscard]] auto IsOpen() const noexcept -> bool;

  // The URL (file name) that is currently open, empty if the decoder
  // is not in an open state.
  [[nodiscard]] auto Url() const noexcept -> const std::string &;

  // Reset the state of the decoder, clearing all cached information putting the
  // decoder in a state as if no file has been opened.
  void Close() noexcept;

  // Returns the video stream index considered to be "best", which is implementation defined.
  // If no such index can be determined returns a negative number. For instance, this could happen
  // if the opened file contains no video streams, or if no file has been opened.
  [[nodiscard]] auto BestVideoStreamIndex() const noexcept -> int;

  // Returns the header information for the video streams in the currently opened file.
  // The list an empty list if no file has been opened.
  //
  // Note that some properties, such as pixel format, may differ from the decoded frames,
  // since those are potentially pushed through a filter graph.
  [[nodiscard]] auto VideoStreamHeaders() const -> const std::vector<InputVideoStreamHeader> &;


  [[nodiscard]] auto VideoStreamHeader(int stream_index) const
    -> std::optional<InputVideoStreamHeader>;

  [[nodiscard]] auto
    DecodeVideoFrame(int stream_index, int frame_nb, DecodedVideoFrame &dvf) noexcept -> bool;

private:
  const DecoderImpl *Pimpl() const { return _pimpl.get(); }
  DecoderImpl *Pimpl() { return _pimpl.get(); }

  std::unique_ptr<DecoderImpl> _pimpl;
};

}// namespace ilp_movie
