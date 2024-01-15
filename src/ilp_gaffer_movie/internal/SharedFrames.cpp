#include "internal/SharedFrames.h"

#include <cassert>// assert

#include <boost/functional/hash.hpp>// boost::hash_combine

#include "internal/LRUCache.h"// IECorePreview::LRUCache
#include "internal/SharedDecoders.h"

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

      const auto decoderEntry =
        IlpGafferMovie::shared_decoders_internal::SharedDecoders::get(key.decoder_key);
      if (decoderEntry.decoder == nullptr) {
        result.error = std::make_shared<std::string>("Bad decoder");
        return result;
      }

      try {
        auto frame = std::make_unique<ilp_movie::Frame>();
        if (!decoderEntry.decoder->DecodeVideoFrame(
              key.video_stream_index, key.frame_nb, /*out*/ *frame)) {
          result.error = std::make_shared<std::string>("Cannot seek to frame");
          return result;
        }
        result.frame.reset(frame.release());// NOLINT
      } catch (std::exception &ex) {
        result.error = std::make_shared<std::string>(ex.what());
        return result;
      }
      return result;
    },
    /*maxCost=*/200
  };
  return cache;
}

}// namespace

namespace IlpGafferMovie::shared_frames_internal {

bool operator==(const FrameCacheKey &lhs, const FrameCacheKey &rhs) noexcept
{
  // clang-format off
  return 
    lhs.decoder_key.fileName == rhs.decoder_key.fileName && 
    lhs.decoder_key.filterGraphDescr.filter_descr == 
        rhs.decoder_key.filterGraphDescr.filter_descr && 
    lhs.decoder_key.filterGraphDescr.out_pix_fmt_name == 
        rhs.decoder_key.filterGraphDescr.out_pix_fmt_name &&
    lhs.video_stream_index == rhs.video_stream_index && 
    lhs.frame_nb == rhs.frame_nb;
  // clang-format on
}

std::size_t hash_value(const FrameCacheKey &k)
{
  std::size_t seed = 0;
  boost::hash_combine(/*out*/ seed, k.decoder_key.fileName);
  boost::hash_combine(/*out*/ seed, k.decoder_key.filterGraphDescr.filter_descr);
  boost::hash_combine(/*out*/ seed, k.decoder_key.filterGraphDescr.out_pix_fmt_name);
  boost::hash_combine(/*out*/ seed, k.video_stream_index);
  boost::hash_combine(/*out*/ seed, k.frame_nb);
  return seed;
}

FrameCacheEntry SharedFrames::get(const FrameCacheKey &key) { return cache().get(key); }

void SharedFrames::erase(const FrameCacheKey &key) { cache().erase(key); }

void SharedFrames::clear() { cache().clear(); }

void SharedFrames::setMaxFrames(const size_t numFrames) { cache().setMaxCost(numFrames); }

size_t SharedFrames::getMaxFrames() { return cache().getMaxCost(); }

size_t SharedFrames::numFrames() { return cache().currentCost(); }

}// namespace IlpGafferMovie::shared_frames_internal