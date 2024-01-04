#include "ilp_movie/frame.hpp"

#include <cassert>// assert

extern "C" {
#include <libavutil/imgutils.h>// av_image_fill_arrays
#include <libavutil/pixdesc.h>// av_get_pix_fmt, av_pix_fmt_desc_get, etc.
}// extern C

#if 0
#include <type_traits>// std::remove_pointer_t

template<typename PixelT>
[[nodiscard]] static constexpr auto PlanePixelDataImpl(const int w,
  const int h,
  const int plane,
  PixelT *const buf,
  const std::size_t buf_count) noexcept
  -> ilp_movie::PixelData<std::remove_pointer_t<decltype(buf)>>
{
  // clang-format off
  if (!(w > 0 && h > 0 &&
        plane >= 0 &&
        buf != nullptr && buf_count > 0U)) {
    return {};
  }
  // clang-format on

  const auto count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
  const auto plane_offset = count * static_cast<std::size_t>(plane);

  if (!(plane_offset + count <= buf_count)) { return {}; }
  return { /*.data=*/&(buf[plane_offset]), /*.count=*/count };// NOLINT
}
static_assert([]() {
  // clang-format off
  constexpr int w = 4;
  constexpr int h = 3;
  constexpr int p = 3;
  std::array<float, w * h * p> buf{};
  return  Empty(PlanePixelDataImpl(w, h, -1, buf.data(), buf.size())) &&
         !Empty(PlanePixelDataImpl(w, h,  0, buf.data(), buf.size())) && 
         !Empty(PlanePixelDataImpl(w, h,  1, buf.data(), buf.size())) &&  
         !Empty(PlanePixelDataImpl(w, h,  2, buf.data(), buf.size())) && 
          Empty(PlanePixelDataImpl(w, h,  3, buf.data(), buf.size()));
  // clang-format on
}());
#endif

namespace ilp_movie {

auto GetBufferSize(const char *pix_fmt_name,
  const int width,
  const int height) noexcept -> std::optional<std::size_t> {

  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return std::nullopt; }

  const int ret = av_image_get_buffer_size(pix_fmt, width, height, /*align=*/1);
  if (ret < 0) { return std::nullopt; }
  return static_cast<std::size_t>(ret);
}

auto CountPlanes(const char *pix_fmt_name) noexcept -> std::optional<int>
{
  if (pix_fmt_name == nullptr) { return std::nullopt; }
  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return std::nullopt; }
  const int n = av_pix_fmt_count_planes(pix_fmt);
  if (n < 0) { return std::nullopt; }
  return n;
}

auto FillArrays(std::array<uint8_t *, 4> &data,
  std::array<int, 4> &linesize,
  const uint8_t *buf,
  const char *pix_fmt_name,
  int width,
  int height) noexcept -> bool
{
  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return false; }

  const int ret =
    av_image_fill_arrays(data.data(), linesize.data(), buf, pix_fmt, width, height, /*align=*/1);
  return ret >= 0;
}

template<>
auto CompPixelData<float>(const std::array<uint8_t *, 4> &data,
  const Comp::ValueType c,
  const int w,
  const int h,
  const char *const pix_fmt_name) noexcept -> PixelData<float>
{
  if (pix_fmt_name == nullptr || c == Comp::kUnknown) { return {}; }

  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return {}; }

  const AVPixFmtDescriptor *const pix_desc = av_pix_fmt_desc_get(pix_fmt);
  if (pix_desc == nullptr) { return {}; }
  assert(1U <= pix_desc->nb_components && pix_desc->nb_components <= 4U);// NOLINT
  if (!(0 <= c && c < static_cast<Comp::ValueType>(pix_desc->nb_components))) { return {}; }

  // Check flags.
  uint64_t mask = 0U;
  mask |= AV_PIX_FMT_FLAG_RGB;// NOLINT
  mask |= AV_PIX_FMT_FLAG_FLOAT;// NOLINT
  mask |= AV_PIX_FMT_FLAG_PLANAR;// NOLINT
  if ((pix_desc->flags & mask) != mask) { return {}; }

  // Check 32-bit floats.
  if (!(pix_desc->comp[c].depth == 32)) { return {}; }// NOLINT

  // Which plane contains the requested component?
  const int p = pix_desc->comp[c].plane;// NOLINT
  assert(0 <= p && p < 4);// NOLINT

  float *ptr = reinterpret_cast<float *>(data.at(static_cast<size_t>(p)));// NOLINT
  if (ptr == nullptr) { return {}; }
  const std::size_t count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

  return { /*.data=*/ptr, /*.count=*/count };
}

template<>
auto CompPixelData<const float>(const std::array<uint8_t *, 4> &data,
  const Comp::ValueType c,
  const int w,
  const int h,
  const char *const pix_fmt_name) noexcept -> PixelData<const float>
{
  // Convert mutable pointer to const pointer.
  auto p = CompPixelData<float>(data, c, w, h, pix_fmt_name);
  return { p.data, p.count };
}


}// namespace ilp_movie
