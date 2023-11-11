#pragma once

#include <cstddef>// std::size_t

#include <ilp_movie/ilp_movie_export.hpp>// ILP_MOVIE_EXPORT
#include <ilp_movie/pixel_format.hpp>// ilp_movie::Channel

namespace ilp_movie {

struct DecodedVideoFrame;

// Simple span.
template<typename PixelT = const float> struct PixelData
{
  PixelT *data = nullptr;
  std::size_t count = 0U;// Number of elements.
};

template<typename PixelT>
[[nodiscard]] ILP_MOVIE_EXPORT constexpr auto Empty(const PixelData<PixelT> &p) noexcept -> bool
{
  return p.data == nullptr || p.count == 0;
}

[[nodiscard]] ILP_MOVIE_EXPORT auto
  ChannelData(int w, int h, Channel ch, const float *buf, std::size_t buf_count) noexcept
  -> PixelData<const float>;

[[nodiscard]] ILP_MOVIE_EXPORT auto
  ChannelData(int w, int h, Channel ch, float *buf, std::size_t buf_count) noexcept
  -> PixelData<float>;

[[nodiscard]] ILP_MOVIE_EXPORT auto ChannelData(const DecodedVideoFrame &dvf, Channel ch) noexcept
  -> PixelData<const float>;

[[nodiscard]] ILP_MOVIE_EXPORT auto ChannelData(DecodedVideoFrame *dvf, Channel ch) noexcept
  -> PixelData<float>;

}// namespace ilp_movie
