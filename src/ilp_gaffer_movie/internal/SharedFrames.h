#pragma once

#include <cstddef>// std::size_t, size_t
#include <memory>// std::shared_ptr
#include <string>// std::string

#include "ilp_gaffer_movie/ilp_gaffer_movie_export.hpp"

#include "ilp_movie/frame.hpp"// ilp_movie::Frame

#include "internal/SharedDecoders.h"

namespace IlpGafferMovie {
namespace shared_frames_internal {

  struct FrameCacheKey
  {
    //ilp_movie::Decoder *decoder = nullptr;
    shared_decoders_internal::DecoderCacheKey decoder_key;
    int video_stream_index = -1;
    int frame_nb = -1;
  };

  // For success, frame should be set, and error left null.
  // For failure, frame should be left null, and error should be set.
  struct FrameCacheEntry
  {
    std::shared_ptr<ilp_movie::Frame> frame;
    std::shared_ptr<std::string> error;
  };

  class ILPGAFFERMOVIE_NO_EXPORT SharedFrames
  {
  public:
    // Creates a frame using a cache, so you don't end up decoding the same
    // frame multiple times.
    static FrameCacheEntry get(const FrameCacheKey &key);

    // Erase a single frame from the cache.
    static void erase(const FrameCacheKey &key);

    // Clear the entire cache.
    static void clear();

    // Sets the limit for the number of frames that will
    // be cached internally.
    static void setMaxFrames(size_t numFrames);

    // Returns the limit for the number of frames that will
    // be cached internally.
    static size_t getMaxFrames();

    // Returns the number of frames currently in the cache.
    static size_t numFrames();
  };

}// namespace shared_frames_internal
}// namespace IlpGafferMovie
