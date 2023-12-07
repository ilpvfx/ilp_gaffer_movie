#include <ilp_movie/pixel_data.hpp>

#include <cassert>// assert
#include <limits>// std::numeric_limits
#include <optional>// std::optional
#include <type_traits>// std::remove_pointer_t

#include <ilp_movie/decoder.hpp>// ilp_movie::DecodedVideoFrame

[[nodiscard]] static constexpr auto ChannelIndex(const ilp_movie::Channel ch) noexcept
  -> std::optional<std::size_t>
{
  std::optional<std::size_t> i = {};
  // clang-format off
  switch (ch) {
  case ilp_movie::Channel::kGray:  
  case ilp_movie::Channel::kRed:   i = 0; break;
  case ilp_movie::Channel::kGreen: i = 1; break;
  case ilp_movie::Channel::kBlue:  i = 2; break;
  case ilp_movie::Channel::kAlpha: i = 3; break;
  default: break;
  }
  // clang-format on
  return i;
}

[[nodiscard]] static constexpr auto ImageSize(const int w, const int h) noexcept
  -> std::optional<std::size_t>
{
  if (!(w > 0 && h > 0)) { return std::nullopt; }
  return static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
}

[[nodiscard]] static constexpr auto PixelDataOffset(const std::size_t image_size,
  const std::size_t channel_index) noexcept -> std::size_t
{
  return image_size * channel_index;
}

template<typename PixelT>
[[nodiscard]] static constexpr auto ChannelDataImpl(const int w,
  const int h,
  const ilp_movie::Channel ch,
  PixelT *const buf,
  const std::size_t buf_count) noexcept
  -> ilp_movie::PixelData<std::remove_pointer_t<decltype(buf)>>
{
  const auto image_size = ImageSize(w, h);
  if (!image_size.has_value()) { return {}; }
  const auto ch_index = ChannelIndex(ch);
  if (!ch_index.has_value()) { return {}; }
  const std::size_t p_offset = PixelDataOffset(*image_size, *ch_index);
  if (!(p_offset < buf_count)) { return {}; }
  assert(buf != nullptr);// NOLINT
  return { /*.data=*/&(buf[p_offset]), /*.count=*/*image_size };// NOLINT
}

namespace ilp_movie {

auto ChannelData(const int w,
  const int h,
  const Channel ch,
  const float *const buf,
  const std::size_t buf_count) noexcept -> PixelData<const float>
{
  return ChannelDataImpl(w, h, ch, buf, buf_count);
}

auto ChannelData(const int w,
  const int h,
  const Channel ch,
  float *const buf,
  const std::size_t buf_count) noexcept -> PixelData<float>
{
  return ChannelDataImpl(w, h, ch, buf, buf_count);
}

auto ChannelData(const DecodedVideoFrame &dvf, const Channel ch) noexcept -> PixelData<const float>
{
  return ChannelData(
    dvf.width, dvf.height, ch, static_cast<const float *>(dvf.buf.data.get()), dvf.buf.count);
}

auto ChannelData(DecodedVideoFrame *const dvf, const Channel ch) noexcept -> PixelData<float>
{
  assert(dvf != nullptr);// NOLINT
  return ChannelData(dvf->width, dvf->height, ch, dvf->buf.data.get(), dvf->buf.count);
}

}// namespace ilp_movie