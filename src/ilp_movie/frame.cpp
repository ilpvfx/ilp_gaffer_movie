#include "ilp_movie/frame.hpp"

#include <cassert>// assert

extern "C" {
#include <libavutil/imgutils.h>// av_image_fill_arrays
#include <libavutil/pixdesc.h>// av_get_pix_fmt, av_pix_fmt_desc_get, etc.
}// extern C

namespace ilp_movie {

auto GetBufferSize(const char *pix_fmt_name, const int width, const int height) noexcept
  -> std::optional<std::size_t>
{

  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return std::nullopt; }

  const int ret = av_image_get_buffer_size(pix_fmt, width, height, /*align=*/1);
  if (ret < 0) { return std::nullopt; }
  return static_cast<std::size_t>(ret);
}

auto FillArrays(std::array<uint8_t *, 4> &data,
  std::array<int, 4> &linesize,
  const uint8_t *buf,
  const char *pix_fmt_name,
  const int width,
  const int height) noexcept -> bool
{
  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return false; }

  return av_image_fill_arrays(
           data.data(), linesize.data(), buf, pix_fmt, width, height, /*align=*/1)
         >= 0;
}

template<>
auto CompPixelData<float>(const std::array<uint8_t *, 4> &data,
  const std::array<int, 4> &linesize,
  const Comp::ValueType c,
  const int height,
  const char *const pix_fmt_name) noexcept -> PixelData<float>
{
  if (pix_fmt_name == nullptr || c == Comp::kUnknown) { return {}; }

  const AVPixelFormat pix_fmt = av_get_pix_fmt(pix_fmt_name);
  if (pix_fmt == AV_PIX_FMT_NONE) { return {}; }

  const AVPixFmtDescriptor *const pix_desc = av_pix_fmt_desc_get(pix_fmt);
  if (pix_desc == nullptr) { return {}; }
  assert(1U <= pix_desc->nb_components && pix_desc->nb_components <= 4U);// NOLINT
  if (!(0 <= c && c < static_cast<Comp::ValueType>(pix_desc->nb_components))) { return {}; }

  // Check flags, all must be set in the pixel description.
  uint64_t mask = 0U;
  mask |= AV_PIX_FMT_FLAG_RGB;// NOLINT
  mask |= AV_PIX_FMT_FLAG_FLOAT;// NOLINT
  mask |= AV_PIX_FMT_FLAG_PLANAR;// NOLINT
  if ((pix_desc->flags & mask) != mask) { return {}; }

  // Check 32-bit floats.
  if (!(pix_desc->comp[c].depth == 32)) { return {}; }// NOLINT

  // Which plane contains the requested component?
  const auto p = static_cast<std::size_t>(pix_desc->comp[c].plane);// NOLINT
  assert(0U <= p && p < 4U);// NOLINT

  // Type punning, should use bit_cast if available...
  float *ptr = reinterpret_cast<float *>(data.at(p));// NOLINT
  if (ptr == nullptr) { return {}; }

  // In [bytes], not supporting negative line sizes right now.
  const int line_sz = linesize.at(p);
  if (line_sz <= 0) { return {}; }
  const std::size_t plane_sz =
    static_cast<std::size_t>(height) * (static_cast<std::size_t>(line_sz) / sizeof(float));

  return { /*.data=*/ptr, /*.count=*/plane_sz };
}

template<>
auto CompPixelData<const float>(const std::array<uint8_t *, 4> &data,
  const std::array<int, 4> &linesize,
  const Comp::ValueType c,
  const int height,
  const char *const pix_fmt_name) noexcept -> PixelData<const float>
{
  // Convert mutable pointer to const pointer.
  auto p = CompPixelData<float>(data, linesize, c, height, pix_fmt_name);
  return { p.data, p.count };
}


}// namespace ilp_movie
