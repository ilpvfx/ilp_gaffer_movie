#pragma once

#include <cstddef>// std::size_t
#include <cstdint>// int64_t, etc.
#include <memory>// std::unique_ptr
#include <optional>// std::optional
#include <string>// std::string
#include <vector>// std::vector

#include "ilp_movie/ilp_movie_export.hpp"// ILP_MOVIE_EXPORT

namespace ilp_movie {

struct Frame;

struct InputVideoStreamHeader
{
  int stream_index = -1;

  // One-based frame number range. Seek should return valid frames for the inclusive range:
  // [first, first + count - 1].
  int64_t first_frame_nb = -1;
  int64_t frame_count = 0;

  int width = -1;
  int height = -1;
  struct
  {
    int num = 0;
    int den = 1;
  } frame_rate = {};

  // SAR: pixel_width / pixel_height.
  struct
  {
    int num = 0;
    int den = 0;
  } pixel_aspect_ratio = {};
  struct
  {
    int num = 0;
    int den = 0;
  } display_aspect_ratio = {};

  const char *pix_fmt_name = nullptr;
  const char *color_range_name = nullptr;
  const char *color_space_name = nullptr;
  const char *color_trc_name = nullptr;
  const char *color_primaries_name = nullptr;
};

struct DecoderFilterGraphDescription
{
  // Pass-through (for video), use "anull" for audio.
  std::string filter_descr = "null";

  // The pixel format of the decoded frames after they've been pulled from
  // the filter graph. This string will be converted to a suitable enum internally
  // but we don't want to expose those enum types on this interface.
  //
  // E.g. "gbrpf32le" for 32-bit float (planar) RGB frames (little endian).
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

  // Open a file with the given URL (file name) and initialize the internal state
  // required to start decoding frames. Returns true if successful; otherwise false.
  [[nodiscard]] auto Open(const std::string &url,
    const DecoderFilterGraphDescription &dfgd) noexcept -> bool;

  // Returns true if the decoder has been successfully opened and has not been closed since.
  [[nodiscard]] auto IsOpen() const noexcept -> bool;

  // The URL (file name) that is currently open.
  // Empty string if no file is currently open.
  [[nodiscard]] auto Url() const noexcept -> const std::string &;

  // Information about the opened file and the streams within. 
  // Empty string if no file is currently open.
  [[nodiscard]] auto Probe() const noexcept -> const std::string &;

  // Reset the state of the decoder, clearing all cached information and putting the
  // decoder in a state as if no file has been opened.
  void Close() noexcept;

  // Returns the video stream index considered to be "best", which is implementation defined.
  // If no such index can be determined returns a negative number. For instance, this could happen
  // if the opened file contains no video streams, or if no file has been opened.
  [[nodiscard]] auto BestVideoStreamIndex() const noexcept -> std::optional<int>;

  // Returns the video stream headers for the currently opened file.
  // The list is empty if no file is currently open.
  //
  // Note that some properties, such as pixel format, may differ from the decoded frames,
  // since those are potentially pushed through a filter graph.
  //
  // NOTE(tohi): Would be nicer to return std::span<const InputVideoStreamHeader> to
  //             clarify ownership.
  [[nodiscard]] auto VideoStreamHeaders() const noexcept -> const std::vector<InputVideoStreamHeader> &;

  // Returns the stream header for the given zero-based index. If this index doesn't correspond
  // to the stream index of an opened video stream, returns null. Note that this function
  // searches only for video streams.
  //
  // In the special case where -1 is passed as the stream index this function tries
  // to return the stream header for the "best" video stream.
  [[nodiscard]] auto VideoStreamHeader(int stream_index) const noexcept
    -> std::optional<InputVideoStreamHeader>;

  [[nodiscard]] auto
    DecodeVideoFrame(int stream_index, int frame_nb, Frame &frame) noexcept -> bool;

private:
  const DecoderImpl *_Pimpl() const { return _pimpl.get(); }
  DecoderImpl *_Pimpl() { return _pimpl.get(); }

  std::unique_ptr<DecoderImpl> _pimpl;
};

}// namespace ilp_movie
