#include "internal/SharedFrames.h"

#include <boost/functional/hash.hpp>// boost::hash_combine

#include "internal/LRUCache.h"// IECorePreview::LRUCache

namespace {

using CacheKey = IlpGafferMovie::shared_frames_internal::FrameCacheKey;
using CacheEntry = IlpGafferMovie::shared_frames_internal::FrameCacheEntry;
using FrameLRUCache = IECorePreview::LRUCache<CacheKey, CacheEntry>;

FrameLRUCache &cache()
{
  static FrameLRUCache cache{
    [](const CacheKey &key, size_t &cost, const IECore::Canceller * /*canceller*/) {
      cost = 1U;

      CacheEntry result = {};

      if (key.decoder == nullptr) {
        result.error = std::make_shared<std::string>("Bad decoder");
        return result;
      }

      if (!key.decoder->IsOpen()) {
        result.error = std::make_shared<std::string>("Decoder not open");
        return result;
      }

      try {
        auto dvf = std::make_unique<ilp_movie::DecodedVideoFrame>();
        if (!key.decoder->DecodeVideoFrame(key.video_stream_index, key.frame_nb, /*out*/ *dvf)) {
          result.error = std::make_shared<std::string>("Cannot seek to frame");
          return result;
        }
        result.frame.reset(dvf.release());// NOLINT
      } catch (std::exception &) {
        result.error = std::make_shared<std::string>("Cannot initialize decoder");
        return result;
      }
      return result;
    },
    /*maxCost=*/200
  };
  return cache;
}// namespace

}// namespace

namespace IlpGafferMovie::shared_frames_internal {

constexpr bool operator==(const FrameCacheKey &lhs, const FrameCacheKey &rhs) noexcept
{
  // clang-format off
  return 
    lhs.decoder == rhs.decoder && 
    lhs.video_stream_index == rhs.video_stream_index && 
    lhs.frame_nb == rhs.frame_nb;
  // clang-format on
}

std::size_t hash_value(const FrameCacheKey &k)
{
  std::size_t seed = 0;
  boost::hash_combine(seed, k.decoder);// NOLINT
  boost::hash_combine(seed, k.video_stream_index);// NOLINT
  boost::hash_combine(seed, k.frame_nb);// NOLINT
  return seed;
}

FrameCacheEntry SharedFrames::get(const FrameCacheKey &key) { return cache().get(key); }

void SharedFrames::erase(const FrameCacheKey &key) { cache().erase(key); }

void SharedFrames::clear() { cache().clear(); }

void SharedFrames::setMaxFrames(const size_t numFrames) { cache().setMaxCost(numFrames); }

size_t SharedFrames::getMaxFrames() { return cache().getMaxCost(); }

size_t SharedFrames::numFrames() { return cache().currentCost(); }

}// namespace IlpGafferMovie::shared_frames_internal