#pragma once

#include <cstddef>// size_t
#include <memory>// std::shared_ptr
#include <string>// std::string

#include "ilp_gaffer_movie/ilp_gaffer_movie_export.hpp"

#include "ilp_movie/decoder.hpp"

namespace IlpGafferMovie {
namespace shared_decoders_internal {

  struct DecoderEntry
  {
    std::shared_ptr<ilp_movie::Decoder> decoder;
    std::shared_ptr<std::string> error;
  };

  class ILPGAFFERMOVIE_NO_EXPORT SharedDecoders
  {
  public:
    // Creates a decoder using a cache, so you don't end up opening the same
    // file multiple times.
    static DecoderEntry get(const std::string &fileName,
      const std::string &filterDescr,
      const std::string &outPixFmtName);

    // Erase a single decoder from the cache.
    static void erase(const std::string &fileName,
      const std::string &filterDescr,
      const std::string &outPixFmtName);

    // Clear the entire cache.
    static void clear();

    // Sets the limit for the number of decoders that will
    // be cached internally.
    static void setMaxDecoders(size_t numDecoders);

    // Returns the limit for the number of decoders that will
    // be cached internally.
    static size_t getMaxDecoders();

    // Returns the number of decoders currently in the cache.
    static size_t numDecoders();
  };

}// namespace shared_decoders_internal
}// namespace IlpGafferMovie
