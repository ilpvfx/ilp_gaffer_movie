#include "internal/SharedDecoders.h"

#include <boost/functional/hash.hpp>// boost::hash_combine

#include "internal/LRUCache.h"// IECorePreview::LRUCache

namespace {

using CacheKey = IlpGafferMovie::shared_decoders_internal::DecoderCacheKey;
using CacheEntry = IlpGafferMovie::shared_decoders_internal::DecoderCacheEntry;
using DecoderLRUCache = IECorePreview::LRUCache<CacheKey, CacheEntry>;

DecoderLRUCache &cache()
{
  static DecoderLRUCache cache{
    [](const CacheKey &key, size_t &cost, const IECore::Canceller * /*canceller*/) {
      // Each decoder costs exactly one unit.
      cost = 1U;

      CacheEntry result;

      auto decoder = std::make_shared<ilp_movie::Decoder>();
      if (!decoder->Open(key.fileName, key.filterGraphDescr)) {
        result.error = std::make_shared<std::string>("Could not open decoder");
        return result;
      }

      // Only store decoder if we were able to open it.
      // Note that it might still be the case that the decoder cannot find any
      // input video streams in the file.
      result.decoder = decoder;

      return result;
    },
    /*maxCost=*/200
  };
  return cache;
}

}// namespace

namespace IlpGafferMovie::shared_decoders_internal {

bool operator==(const DecoderCacheKey &lhs, const DecoderCacheKey &rhs) noexcept
{
  // clang-format off
  return 
    lhs.fileName == rhs.fileName && 
    lhs.filterGraphDescr.filter_descr == rhs.filterGraphDescr.filter_descr && 
    lhs.filterGraphDescr.out_pix_fmt_name == rhs.filterGraphDescr.out_pix_fmt_name;
  // clang-format on
}

std::size_t hash_value(const DecoderCacheKey &k)
{
  std::size_t seed = 0;
  boost::hash_combine(seed, k.fileName);
  boost::hash_combine(seed, k.filterGraphDescr.filter_descr);
  boost::hash_combine(seed, k.filterGraphDescr.out_pix_fmt_name);
  return seed;
}

DecoderCacheEntry SharedDecoders::get(const DecoderCacheKey &key) { return cache().get(key); }

void SharedDecoders::erase(const DecoderCacheKey &key) { cache().erase(key); }

void SharedDecoders::clear() { cache().clear(); }

void SharedDecoders::setMaxDecoders(const size_t numDecoders) { cache().setMaxCost(numDecoders); }

size_t SharedDecoders::getMaxDecoders() { return cache().getMaxCost(); }

size_t SharedDecoders::numDecoders() { return cache().currentCost(); }

}// namespace IlpGafferMovie::shared_decoders_internal