#include "internal/SharedDecoders.h"

#include <cstddef>// std::size_t

#include <boost/functional/hash.hpp>

#include "internal/LRUCache.h"

namespace {

struct DecoderKey
{
  std::string fileName;
  ilp_movie::DecoderFilterGraphDescription filterGraphDescr;
};

bool operator==(const DecoderKey &lhs, const DecoderKey &rhs) noexcept
{
  // clang-format off
  return 
    lhs.fileName == rhs.fileName && 
    lhs.filterGraphDescr.filter_descr == rhs.filterGraphDescr.filter_descr && 
    lhs.filterGraphDescr.out_pix_fmt_name == rhs.filterGraphDescr.out_pix_fmt_name;
  // clang-format on
}

std::size_t hash_value(const DecoderKey &k)
{
  std::size_t seed = 0;
  boost::hash_combine(seed, k.fileName);
  boost::hash_combine(seed, k.filterGraphDescr.filter_descr);
  boost::hash_combine(seed, k.filterGraphDescr.out_pix_fmt_name);
  return seed;
}


using DecoderEntry = IlpGafferMovie::shared_decoders_internal::DecoderEntry;

DecoderEntry
  fileCacheGetter(const DecoderKey &key, size_t &cost, const IECore::Canceller * /*canceller*/)
{
  // Each decoder costs exactly one unit.
  cost = 1U;

  DecoderEntry result;

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
}

using DecoderLRUCache = IECorePreview::LRUCache<DecoderKey, DecoderEntry>;

DecoderLRUCache &cache()
{
  // TODO(tohi): Do we really need to dynamically allocate the cache??
  static DecoderLRUCache *cache = new DecoderLRUCache(fileCacheGetter, /*maxCost=*/200);// NOLINT
  return *cache;
}

}// namespace

namespace IlpGafferMovie::shared_decoders_internal {

DecoderEntry SharedDecoders::get(const std::string &fileName,
  const std::string &filterDescr,
  const std::string &outPixFmtName)
{
  // NOTE(tohi): There's quite a lot of string copying going on here...
  // clang-format off
  return cache().get(DecoderKey{ 
    /*.fileName=*/fileName,
    /*.filterGraphDescr=*/{ 
      /*.filter_descr=*/filterDescr,
      /*.out_pix_fmt_name=*/outPixFmtName }});
  // clang-format on
}

void SharedDecoders::erase(const std::string &fileName,
  const std::string &filterDescr,
  const std::string &outPixFmtName)
{
  // NOTE(tohi): There's quite a lot of string copying going on here...
  // clang-format off
  cache().erase(DecoderKey{ 
    /*.fileName=*/fileName,
    /*.filterGraphDescr=*/{ 
      /*.filter_descr=*/filterDescr,
      /*.out_pix_fmt_name=*/outPixFmtName }});
  // clang-format on
}

void SharedDecoders::clear() { cache().clear(); }

void SharedDecoders::setMaxDecoders(const size_t numDecoders) { cache().setMaxCost(numDecoders); }

size_t SharedDecoders::getMaxDecoders() { return cache().getMaxCost(); }

size_t SharedDecoders::numDecoders() { return cache().currentCost(); }

}// namespace IlpGafferMovie::shared_decoders_internal