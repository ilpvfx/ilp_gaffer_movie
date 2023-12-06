#include <ilp_gaffer_movie/av_reader.hpp>

// The nested TaskMutex needs to be the first to include tbb
#include "internal/LRUCache.h"
#include "internal/SharedDecoders.h"
#include "internal/trace.hpp"

#include <cassert>// assert
#include <memory>
#include <mutex>// std::call_once
#include <numeric>// std::iota
#include <string>// std::string

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <GafferImage/FormatPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImageReader.h>

#include <Gaffer/Context.h>
#include <Gaffer/StringPlug.h>

// namespace {
// struct FrameCacheKey;
// inline std::size_t hash_value(const FrameCacheKey &k);
// }// namespace

#include <boost/bind/bind.hpp>
#include <boost/functional/hash.hpp>

#include <ilp_movie/decoder.hpp>
#include <ilp_movie/log.hpp>
#include <ilp_movie/pixel_data.hpp>

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::AvReader);

static const IECore::InternedString kTileBatchIndexContextName("__tileBatchIndex");

namespace {

// For success, frame should be set, and error left null.
// For failure, frame should be left null, and error should be set.
struct FrameCacheEntry
{
  std::shared_ptr<ilp_movie::DecodedVideoFrame> frame;
  std::shared_ptr<std::string> error;
};

struct FrameCacheKey
{
  ilp_movie::Decoder *decoder = nullptr;
  int video_stream_index = -1;
  int frame_nb = -1;
};

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
#if 0
  std::size_t seed = 0U;
  // boost::hash_combine(seed, reinterpret_cast<std::uintptr_t>(k.decoder));// NOLINT
  boost::hash_combine(seed, k.decoder);// NOLINT
  boost::hash_combine(seed, k.video_stream_index);// NOLINT
  boost::hash_combine(seed, k.frame_nb);// NOLINT
  return seed;
#else 
  std::size_t seed = 0U;
  boost::hash_combine(seed, k.frame_nb);// NOLINT
  return seed;
#endif
}

FrameCacheEntry
  frameCacheGetter(const FrameCacheKey &key, size_t &cost, const IECore::Canceller * /*canceller*/)
{
  IlpGafferMovie::TRACE("AvReader", "frameCacheGetter");

  cost = 1U;

  FrameCacheEntry result = {};

  if (key.decoder == nullptr) {
    result.error = std::make_shared<std::string>("AvReader : Bad decoder");
    return result;
  }

  if (!key.decoder->IsOpen()) {
    result.error = std::make_shared<std::string>("AvReader : Decoder not open");
    return result;
  }

  try {
    auto dvf = std::make_unique<ilp_movie::DecodedVideoFrame>();
    if (!key.decoder->DecodeVideoFrame(key.video_stream_index, key.frame_nb, /*out*/ *dvf)) {
      result.error = std::make_shared<std::string>("AvReader : Cannot seek to frame");
      return result;
    }
    result.frame.reset(dvf.release());// NOLINT
  } catch (std::exception &) {
    result.error = std::make_shared<std::string>("AvReader : Cannot initialize decoder");
    return result;
  }
  return result;
}

using FrameCache = IECorePreview::LRUCache<FrameCacheKey, FrameCacheEntry>;

FrameCache *frameCache()
{
  static auto *c = new FrameCache(frameCacheGetter, /*maxCost=*/200);
  return c;
}

}// namespace

#if 0
// This function transforms an input region to account for the display window being flipped.
// This is similar to Format::fromEXRSpace/toEXRSpace but those functions mix in switching
// between inclusive/exclusive bounds, so in order to use them we would have to add a bunch
// of confusing offsets by 1.  In this class, we always interpret ranges as [ minPixel,
// onePastMaxPixel )
[[nodiscard]] static auto FlopDisplayWindow(const Imath::Box2i &b,
  const int displayOriginY,
  const int displayHeight) -> Imath::Box2i
{
  return Imath::Box2i(
    Imath::V2i(b.min.x, displayOriginY + displayOriginY + displayHeight - b.max.y),
    Imath::V2i(b.max.x, displayOriginY + displayOriginY + displayHeight - b.min.y));
}
#endif

namespace IlpGafferMovie {

std::size_t AvReader::FirstPlugIndex = 0;

AvReader::AvReader(const std::string &name) : GafferImage::ImageNode{ name }
{
  using Plug = Gaffer::Plug;
  using BoolPlug = Gaffer::BoolPlug;
  using IntPlug = Gaffer::IntPlug;
  using IntVectorDataPlug = Gaffer::IntVectorDataPlug;
  using IntVectorData = IECore::IntVectorData;
  using StringPlug = Gaffer::StringPlug;
  using StringVectorDataPlug = Gaffer::StringVectorDataPlug;
  using StringVectorData = IECore::StringVectorData;
  using ObjectVectorPlug = Gaffer::ObjectVectorPlug;
  using ObjectVector = IECore::ObjectVector;

#if 0
  addChild(new IntPlug("missingFrameMode",
    Plug::In,
    /*defaultValue=*/static_cast<int>(MissingFrameMode::kError),
    /*minValue=*/static_cast<int>(MissingFrameMode::kError),
    /*maxValue=*/static_cast<int>(MissingFrameMode::kHold)));
#endif

  storeIndexOfNextChild(FirstPlugIndex);
  addChild(new StringPlug(// [0]
    /*name=*/"fileName",
    /*direction=*/Plug::In));
  addChild(new IntPlug(// [1]
    /*name=*/"refreshCount",
    /*direction=*/Plug::In));

  addChild(new IntPlug(// [2]
    /*name=*/"videoStreamIndex",
    /*direction=*/Plug::In));
  addChild(new StringPlug(// [3]
    /*name=*/"filterGraph",
    /*direction=*/Plug::In));

  addChild(new StringVectorDataPlug(// [4]
    /*name=*/"availableVideoStreamInfo",
    /*direction=*/Plug::Out,
    /*defaultValue=*/new StringVectorData{ std::vector<std::string>{ "---" } }));
  addChild(new IntVectorDataPlug(// [5]
    /*name=*/"availableVideoStreamIndices",
    /*direction=*/Plug::Out,
    /*defaultValue=*/new IntVectorData{ std::vector<int>{ -1 } }));

  addChild(new IntVectorDataPlug(// [6]
    /*name=*/"availableFrames",
    /*direction=*/Plug::Out,
    /*defaultValue=*/new IntVectorData));
  addChild(new BoolPlug(// [7]
    /*name=*/"fileValid",
    /*direction=*/Plug::Out));

  addChild(new ObjectVectorPlug(// [8]
    /*name=*/"__tileBatch",
    /*direction=*/Plug::Out,
    /*defaultValue=*/new ObjectVector));

  // NOLINTNEXTLINE
  plugSetSignal().connect(boost::bind(&AvReader::_plugSet, this, boost::placeholders::_1));
}

// clang-format off
// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL(name, type, index)          \
  type *AvReader::name()                             \
  {                                                  \
    return getChild<type>(FirstPlugIndex + (index)); \
  }                                                  \
  const type *AvReader::name() const                 \
  {                                                  \
    return getChild<type>(FirstPlugIndex + (index)); \
  }
// clang-format on

PLUG_MEMBER_IMPL(fileNamePlug, Gaffer::StringPlug, 0U);
PLUG_MEMBER_IMPL(refreshCountPlug, Gaffer::IntPlug, 1U);
PLUG_MEMBER_IMPL(videoStreamIndexPlug, Gaffer::IntPlug, 2U);
PLUG_MEMBER_IMPL(filterGraphPlug, Gaffer::StringPlug, 3U);

PLUG_MEMBER_IMPL(availableVideoStreamInfoPlug, Gaffer::StringVectorDataPlug, 4U);
PLUG_MEMBER_IMPL(availableVideoStreamIndicesPlug, Gaffer::IntVectorDataPlug, 5U);
PLUG_MEMBER_IMPL(availableFramesPlug, Gaffer::IntVectorDataPlug, 6U);

PLUG_MEMBER_IMPL(fileValidPlug, Gaffer::BoolPlug, 7U);

PLUG_MEMBER_IMPL(_tileBatchPlug, Gaffer::ObjectVectorPlug, 8U);

#if 0
PLUG_MEMBER_IMPL(missingFrameModePlug, Gaffer::IntPlug, XXX);
#endif

size_t AvReader::supportedExtensions(std::vector<std::string> &extensions)
{
  extensions = ilp_movie::Decoder::SupportedFormats();
  return extensions.size();
}

void AvReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  GafferImage::ImageNode::affects(input, outputs);

  if (input == fileNamePlug() || input == refreshCountPlug()) {
    TRACE("AvReader", "affects - fileName | refreshCount");

    outputs.push_back(fileValidPlug());
    outputs.push_back(availableVideoStreamInfoPlug());
    outputs.push_back(availableVideoStreamIndicesPlug());
  }

  if (input == fileNamePlug() || input == refreshCountPlug() || input == videoStreamIndexPlug()) {
    TRACE("AvReader", "affects - fileName | refreshCount | videoStreamIndex");

    outputs.push_back(availableFramesPlug());

    outputs.push_back(_tileBatchPlug());
    for (Gaffer::ValuePlug::Iterator it(outPlug()); !it.done(); ++it) {
      outputs.push_back(it->get());
    }
  }
}

void AvReader::hash(const Gaffer::ValuePlug *output,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hash(output, context, h);

  // clang-format off
  if (output == fileValidPlug() || 
      output == availableVideoStreamInfoPlug() || 
      output == availableVideoStreamIndicesPlug()) {
    // clang-format on
    TRACE("AvReader", "hash - fileValid | availableVideoStreamInfo | availableVideoStreamIndices");

    fileNamePlug()->hash(h);
    refreshCountPlug()->hash(h);
  } else if (output == availableFramesPlug()) {
    TRACE("AvReader", "hash - availableFrames");

    fileNamePlug()->hash(h);
    refreshCountPlug()->hash(h);
    videoStreamIndexPlug()->hash(h);
  } else if (output == _tileBatchPlug()) {
    TRACE("AvReader", "hash - _tileBatch");

    h.append(context->get<Imath::V3i>(kTileBatchIndexContextName));
    h.append(context->get<std::string>(
      GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));

    Gaffer::Context::EditableScope c(context);
    c.remove(kTileBatchIndexContextName);

    fileNamePlug()->hash(h);
    h.append(context->getFrame());
    refreshCountPlug()->hash(h);
    videoStreamIndexPlug()->hash(h);
    // missingFrameModePlug()->hash(h);
  }
}

void AvReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  using BoolPlug = Gaffer::BoolPlug;
  using IntVectorData = IECore::IntVectorData;
  using IntVectorDataPtr = IECore::IntVectorDataPtr;
  using IntVectorDataPlug = Gaffer::IntVectorDataPlug;
  using StringVectorData = IECore::StringVectorData;
  using StringVectorDataPtr = IECore::StringVectorDataPtr;
  using StringVectorDataPlug = Gaffer::StringVectorDataPlug;

  if (output == fileValidPlug()) {
    TRACE("AvReader", "compute - fileValid");

    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    if (decoder == nullptr) {
      static_cast<BoolPlug *>(output)->setValue(false);// NOLINT
    } else {
      static_cast<BoolPlug *>(output)->setValue(decoder->IsOpen());// NOLINT
    }
  } else if (output == availableFramesPlug()) {
    TRACE("AvReader", "compute - availableFrames");

    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    const int video_stream_index = videoStreamIndexPlug()->getValue();
    if (decoder == nullptr || !decoder->IsOpen() || video_stream_index < 0) {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
    } else {
      auto &&video_stream_headers = decoder->VideoStreamHeaders();
      if (video_stream_headers.empty()) {
        // The file contains no video streams.
        static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
      }

      assert(0 <= video_stream_index// NOLINT
             && video_stream_index < static_cast<int>(video_stream_headers.size()));
      const auto &dvsi = video_stream_headers[static_cast<size_t>(video_stream_index)];
      assert(dvsi.frame_count >= 0);// NOLINT
      IntVectorDataPtr resultData = new IntVectorData;
      auto &result = resultData->writable();
      result.resize(static_cast<size_t>(dvsi.frame_count));
      const int start_value = 1;// TODO(tohi): Could be different?
      std::iota(std::begin(result), std::end(result), start_value);
      static_cast<IntVectorDataPlug *>(output)->setValue(resultData);// NOLINT
    }
  } else if (output == availableVideoStreamInfoPlug()) {
    TRACE("AvReader", "compute - availableVideoStreamInfo");

    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    if (decoder != nullptr && decoder->IsOpen()) {
      auto &&video_stream_headers = decoder->VideoStreamHeaders();
      if (!video_stream_headers.empty()) {
        StringVectorDataPtr resultData = new StringVectorData;
        auto &result = resultData->writable();

        // There should be a video stream tagged as "best", put it first in the list.
        for (auto &&ivsh : video_stream_headers) {
          if (ivsh.is_best) {
            result.emplace_back((boost::format{ "Best (stream #%1%)" } % ivsh.stream_index).str());
          }
        }

        // Then fill in the rest of the streams (including the one that was tagged as "best").
        for (auto &&vsi : video_stream_headers) {
          result.emplace_back((boost::format{ "stream #%1%" } % vsi.stream_index).str());
        }

        static_cast<StringVectorDataPlug *>(output)->setValue(resultData);// NOLINT
      } else {
        static_cast<StringVectorDataPlug *>(output)->setToDefault();// NOLINT
      }
    } else {
      static_cast<StringVectorDataPlug *>(output)->setToDefault();// NOLINT
    }
  } else if (output == availableVideoStreamIndicesPlug()) {
    TRACE("AvReader", "compute - availableVideoStreamIndices");

    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    if (decoder != nullptr && decoder->IsOpen()) {
      // auto&& video_stream_headers = decoder->VideoStreamHeaders();

      IntVectorDataPtr resultData = new IntVectorData;
      auto &result = resultData->writable();

      // for (auto &&vsi : video_stream_info) { result.emplace_back(vsi.stream_index); }
      result.emplace_back(-1);
      result.emplace_back(0);
      result.emplace_back(2);

      static_cast<IntVectorDataPlug *>(output)->setValue(resultData);// NOLINT
    } else {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
    }
#if 0
    if (_decoder->IsOpen()) {
      IntVectorDataPtr resultData = new IntVectorData;
      auto &result = resultData->writable();

      const auto video_stream_info = _decoder->VideoStreamInfo();
      if (!video_stream_info.empty()) {
        result.reserve(video_stream_info.size());
        for (auto &&vsi : video_stream_info) {
          if (vsi.is_best) { result.emplace_back(vsi.stream_index); }
        }

        for (auto &&vsi : video_stream_info) { result.emplace_back(vsi.stream_index); }

        static_cast<IntVectorDataPlug *>(output)->setValue(resultData);// NOLINT
      } else {
        static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
      }
    } else {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
    }
#endif
  }
#if 0
  else if (output == _tileBatchPlug()) {
    TRACE("compute - tileBatch");
    // const auto tileBatchIndex = context->get<Imath::V3i>(kTileBatchIndexContextName);

    Gaffer::Context::EditableScope c(context);
    c.remove(kTileBatchIndexContextName);

    std::shared_ptr<Frame> frame = std::static_pointer_cast<Frame>(_retrieveFile(c.context()));
    if (frame == nullptr) {
      throw IECore::Exception(
        "OpenImageIOReader - trying to evaluate _tileBatchPlug() with invalid file, this should "
        "never happen.");
    }

    static_cast<Gaffer::ObjectVectorPlug *>(output)->setValue(
      frame->readTileBatch(context, tileBatchIndex));
  }
#endif
  else {
    // TRACE("AvReader", "compute - ImageNode");
    ImageNode::compute(output, context);
  }
}

Gaffer::ValuePlug::CachePolicy AvReader::computeCachePolicy(const Gaffer::ValuePlug *output) const
{
  if (output == _tileBatchPlug()) {
    // Request blocking compute for tile batches, to avoid concurrent threads loading
    // the same batch redundantly.
    return Gaffer::ValuePlug::CachePolicy::Standard;
  } else if (output == outPlug()->channelDataPlug()) {
    // Disable caching on channelDataPlug, since it is just a redirect to the correct tile of
    // the private tileBatchPlug, which is already being cached.
    return Gaffer::ValuePlug::CachePolicy::Uncached;
  }
  return GafferImage::ImageNode::computeCachePolicy(output);
}

void AvReader::hashViewNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  // We always return the same default list of view names, so
  // nothing interesting to hash here.
  GafferImage::ImageNode::hashViewNames(parent, context, h);
}

IECore::ConstStringVectorDataPtr AvReader::computeViewNames(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // This seems to be an OpenEXR thing. We just return a vector with a single string:
  // { "default" }.
  return GafferImage::ImagePlug::defaultViewNames();
}

void AvReader::hashFormat(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  // We don't need to depend on the current frame here, since we rely on the stream
  // headers to provide the format for a given video stream index.

  ImageNode::hashFormat(parent, context, h);
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  videoStreamIndexPlug()->hash(h);

  // missingFrameModePlug()->hash(h);
  const auto format = GafferImage::FormatPlug::getDefaultFormat(context);
  h.append(format.getDisplayWindow());
  h.append(format.getPixelAspect());
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

GafferImage::Format AvReader::computeFormat(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // Use information from the stream headers cached by the decoder, avoid retrieving a frame here
  // since that could be quite expensive.

  const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
  if (decoder == nullptr || !decoder->IsOpen()) {
    return GafferImage::FormatPlug::getDefaultFormat(context);
  }

  auto &&video_stream_headers = decoder->VideoStreamHeaders();
  if (video_stream_headers.empty()) {
    // No video streams in opened file.
    return GafferImage::FormatPlug::getDefaultFormat(context);
  }

  int video_stream_index = videoStreamIndexPlug()->getValue();
  if (video_stream_index < 0) {
    video_stream_index = decoder->BestVideoStreamIndex();
    if (video_stream_index < 0) {
      // We have video stream headers, but no "best" stream index.
      // This is a weird case.
      return GafferImage::FormatPlug::getDefaultFormat(context);
    }
  }

  const auto &dvsi = video_stream_headers[static_cast<std::size_t>(video_stream_index)];// NOLINT

  const double pixel_aspect =
    (dvsi.pixel_aspect_ratio.num > 0 && dvsi.pixel_aspect_ratio.den > 0)
      ? static_cast<double>(dvsi.pixel_aspect_ratio.num) / dvsi.pixel_aspect_ratio.den
      : 1.0;

  // clang-format off
  return GafferImage::Format{ 
    /*displayWindow=*/Imath::Box2i{ 
      /*minT=*/Imath::V2i{ 0, 0 },
      /*maxT=*/Imath::V2i{ dvsi.width, dvsi.height } },
    /*pixelAspect=*/pixel_aspect };
  // clang-format on  
}

void AvReader::hashDataWindow(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  // We don't need to depend on the current frame here, since we rely on the stream
  // headers to provide the data window for a given video stream index.

  ImageNode::hashDataWindow(parent, context, h);
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  videoStreamIndexPlug()->hash(h);

  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

Imath::Box2i AvReader::computeDataWindow(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  TRACE("AvReader", "computeDataWindow");

  const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
  if (decoder == nullptr || !decoder->IsOpen()) { return parent->dataWindowPlug()->defaultValue(); }

  auto&& video_stream_headers = decoder->VideoStreamHeaders();
  if (video_stream_headers.empty()) { return parent->dataWindowPlug()->defaultValue(); }

  int video_stream_index = videoStreamIndexPlug()->getValue();
  if (video_stream_index < 0) {
    video_stream_index = decoder->BestVideoStreamIndex();
    if (video_stream_index < 0) { 
      // We have video stream headers, but no "best" stream index. 
      // This is a weird case.
      return parent->dataWindowPlug()->defaultValue(); 
    }
  }

  auto && dvsi = video_stream_headers[static_cast<std::size_t>(video_stream_index)];// NOLINT
#if 1
  // clang-format off
  return Imath::Box2i{
    /*minT=*/Imath::V2i{0, 0}, 
    /*maxT=*/Imath::V2i{dvsi.width, dvsi.height}
  };
  // clang-format on
#else
  const auto data_window = Imath::Box2i{ /*minT=*/Imath::V2i{ 0, 0 },
    /*maxT=*/Imath::V2i{ frame->width, frame->height } };
  return FlopDisplayWindow(data_window, 0, frame->height);
#endif
}

void AvReader::hashMetadata(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashMetadata(parent, context, h);
  fileNamePlug()->hash(h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstCompoundDataPtr AvReader::computeMetadata(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("AvReader", "computeMetadata");

  // if (file == nullptr) {
  //   return parent->dataWindowPlug()->defaultValue();
  // }

  const auto dataType = std::string{ "uint32" };
  const auto fileFormat = std::string{ std::string{ "mov" } /*file->formatName()*/ };

  IECore::CompoundDataPtr result = new IECore::CompoundData;
  result->writable()["dataType"] = new IECore::StringData(dataType);// NOLINT
  result->writable()["fileFormat"] = new IECore::StringData(fileFormat);// NOLINT
  // Any other custom metadata attributes from the file...
  return result;
}

void AvReader::hashDeep(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashDeep(parent, context, h);
  // fileNamePlug()->hash(h);
  // h.append(context->getFrame());
  // refreshCountPlug()->hash(h);
  // // missingFrameModePlug()->hash(h);
  // h.append(context->get<std::string>(
  //   GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

bool AvReader::computeDeep(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // Always false for video, this is an EXR thing.
  return false;
}

void AvReader::hashSampleOffsets(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  using ImagePlug = GafferImage::ImagePlug;

  GafferImage::ImageNode::hashSampleOffsets(parent, context, h);

  h.append(context->get<Imath::V2i>(ImagePlug::tileOriginContextName));
  h.append(context->get<std::string>(ImagePlug::viewNameContextName, ImagePlug::defaultViewName));

  {
    ImagePlug::GlobalScope c(context);
    fileNamePlug()->hash(h);
    h.append(context->getFrame());
    refreshCountPlug()->hash(h);
    // missingFrameModePlug()->hash(h);
  }
}

IECore::ConstIntVectorDataPtr AvReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // We don't have to worry about "deep" images or tile batches since we are not dealing with EXR
  // images.
  return GafferImage::ImagePlug::flatTileSampleOffsets();
}

void AvReader::hashChannelNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashChannelNames(parent, context, h);
  fileNamePlug()->hash(h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstStringVectorDataPtr AvReader::computeChannelNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  TRACE("AvReader", "computeChannelNames");

  auto frame = std::static_pointer_cast<ilp_movie::DecodedVideoFrame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->channelNamesPlug()->defaultValue(); }

  // clang-format off
  IECore::StringVectorDataPtr channelNamesData = nullptr;
  switch (frame->pix_fmt) {
  case ilp_movie::PixelFormat::kGray:
  case ilp_movie::PixelFormat::kRGB:
    channelNamesData = new IECore::StringVectorData({// NOLINT
      GafferImage::ImageAlgo::channelNameR,
      GafferImage::ImageAlgo::channelNameG,
      GafferImage::ImageAlgo::channelNameB,
    });
    break;
  case ilp_movie::PixelFormat::kRGBA:
    channelNamesData = new IECore::StringVectorData({// NOLINT
      GafferImage::ImageAlgo::channelNameR,
      GafferImage::ImageAlgo::channelNameG,
      GafferImage::ImageAlgo::channelNameB,
      GafferImage::ImageAlgo::channelNameA,
    });
    break;
  case ilp_movie::PixelFormat::kNone:
  default:
    throw IECore::Exception(boost::str(boost::format("Unspecified frame pixel format")));
  }
  // clang-format on
  return channelNamesData;
}

void AvReader::hashChannelData(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashChannelData(parent, context, h);
  h.append(context->get<Imath::V2i>(GafferImage::ImagePlug::tileOriginContextName));
  h.append(context->get<std::string>(GafferImage::ImagePlug::channelNameContextName));
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));

  {
    GafferImage::ImagePlug::GlobalScope c(context);
    fileNamePlug()->hash(h);
    h.append(context->getFrame());
    refreshCountPlug()->hash(h);
    videoStreamIndexPlug()->hash(h);
    // missingFrameModePlug()->hash(h);
  }
}

IECore::ConstFloatVectorDataPtr AvReader::computeChannelData(const std::string &channelName,
  const Imath::V2i &tileOrigin,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  // IECore::msg(IECore::Msg::Info,
  //   "AvReader",
  //   boost::format("computeChannelData - channelName = %1%, tileOrigin = (%2%, %3%)") %
  //   channelName
  //     % tileOrigin.x % tileOrigin.y);

  GafferImage::ImagePlug::GlobalScope c(context);
  auto frame = std::static_pointer_cast<ilp_movie::DecodedVideoFrame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->channelDataPlug()->defaultValue(); }

  const Imath::Box2i dataWindow = outPlug()->dataWindowPlug()->getValue();
  const Imath::Box2i tileBound(
    /*minT=*/tileOrigin,
    /*maxT=*/tileOrigin + Imath::V2i(GafferImage::ImagePlug::tileSize()));
  if (!GafferImage::BufferAlgo::intersects(dataWindow, tileBound)) {
    throw IECore::Exception(
      boost::str(boost::format("AvReader : Invalid tile (%i,%i) -> (%i,%i) not within "
                               "data window (%i,%i) -> (%i,%i).")
                 % tileBound.min.x % tileBound.min.y % tileBound.max.x % tileBound.max.y
                 % dataWindow.min.x % dataWindow.min.y % dataWindow.max.x % dataWindow.max.y));
  }

  // c.set(kTileBatchIndexContextName, &tileBatchIndex);

  // ConstObjectVectorPtr tileBatch = tileBatchPlug()->getValue();
  IECore::FloatVectorDataPtr tileData = new IECore::FloatVectorData(
    std::vector<float>(static_cast<size_t>(GafferImage::ImagePlug::tilePixels())));
  auto &tile = tileData->writable();

  const auto *f = frame.get();
  ilp_movie::PixelData<float> ch = {};
  switch (frame->pix_fmt) {
  case ilp_movie::PixelFormat::kGray:
    ch = ilp_movie::ChannelData(
      f->width, f->height, ilp_movie::Channel::kRed, f->buf.data.get(), f->buf.count);
    break;
  case ilp_movie::PixelFormat::kRGB:
    if (channelName == GafferImage::ImageAlgo::channelNameR) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kRed, f->buf.data.get(), f->buf.count);
    } else if (channelName == GafferImage::ImageAlgo::channelNameG) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kGreen, f->buf.data.get(), f->buf.count);
    } else if (channelName == GafferImage::ImageAlgo::channelNameB) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kBlue, f->buf.data.get(), f->buf.count);
    } else {
      throw IECore::Exception("Unexpected channel name");
    }
    break;
  case ilp_movie::PixelFormat::kRGBA:
    if (channelName == GafferImage::ImageAlgo::channelNameR) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kRed, f->buf.data.get(), f->buf.count);
    } else if (channelName == GafferImage::ImageAlgo::channelNameG) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kGreen, f->buf.data.get(), f->buf.count);
    } else if (channelName == GafferImage::ImageAlgo::channelNameB) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kBlue, f->buf.data.get(), f->buf.count);
    } else if (channelName == GafferImage::ImageAlgo::channelNameA) {
      ch = ilp_movie::ChannelData(
        f->width, f->height, ilp_movie::Channel::kAlpha, f->buf.data.get(), f->buf.count);
    } else {
      throw IECore::Exception("Unexpected channel name");
    }
    break;
  case ilp_movie::PixelFormat::kNone:
  default:
    throw IECore::Exception("Unspecified frame pixel format");
  }

  const Imath::Box2i tileRegion = GafferImage::BufferAlgo::intersection(tileBound, dataWindow);

  constexpr auto kTileSize = static_cast<size_t>(GafferImage::ImagePlug::tileSize());
  for (int y = tileRegion.min.y; y < tileRegion.max.y; ++y) {
    float *dst = &tile[static_cast<size_t>(y - tileRegion.min.y) * kTileSize];
    const float *src = &(ch.data[static_cast<size_t>(y) * static_cast<size_t>(f->width)// NOLINT
                                 + static_cast<size_t>(tileRegion.min.x)]);
    std::memcpy(dst, src, sizeof(float) * static_cast<size_t>(tileRegion.max.x - tileRegion.min.x));
  }

  return tileData;
}

std::once_flag log_init_flag;

std::shared_ptr<void> AvReader::_retrieveDecoder(const Gaffer::Context * /*context*/) const
{
  TRACE("AvReader",
    (boost::format("_retrieveDecoder: '%1%'") % fileNamePlug()->getValue().c_str()).str());

  // Initialize logging so that Gaffer picks up log messages from the ilp_movie function calls.
  std::call_once(log_init_flag, []() {
    ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);
    ilp_movie::SetLogCallback([](int level, const char *s) {
      auto iec_level = IECore::MessageHandler::Level::Invalid;
      // clang-format off
      switch (level) {
      case ilp_movie::LogLevel::kPanic:
      case ilp_movie::LogLevel::kFatal:
      case ilp_movie::LogLevel::kError:
        iec_level = IECore::MessageHandler::Level::Error; break;
      case ilp_movie::LogLevel::kWarning:
        iec_level = IECore::MessageHandler::Level::Warning; break;
      case ilp_movie::LogLevel::kInfo:
      case ilp_movie::LogLevel::kVerbose:
      case ilp_movie::LogLevel::kDebug:
      case ilp_movie::LogLevel::kTrace:
        iec_level = IECore::MessageHandler::Level::Info; break;
      case ilp_movie::LogLevel::kQuiet: // ???
      default: 
        break;
      }
      // clang-format on

      // Remove trailing newline character since the IECore logger will add one.
      auto str = std::string{ s };
      if (!str.empty() && *str.rbegin() == '\n') { str.erase(str.length() - 1); }
      IECore::msg(iec_level, "AvReader", str);
    });
  });

  const std::string fileName = fileNamePlug()->getValue();
  if (fileName.empty()) { return nullptr; }

  const std::string filterGraph = filterGraphPlug()->getValue();
  static const std::string kOutPixFmt = "gbrpf32le";

  auto decoderEntry =
    shared_decoders_internal::SharedDecoders::get(fileName, filterGraph, kOutPixFmt);
  if (decoderEntry.decoder == nullptr) { throw IECore::Exception(*(decoderEntry.error)); }

  return decoderEntry.decoder;
}

std::shared_ptr<void> AvReader::_retrieveFrame(const Gaffer::Context *context/*,
  bool holdForBlack = false*/) const
{
#if 0
  TRACE("AvReader",
    (boost::format("_retrieveFrame: '%1%'") % fileNamePlug()->getValue().c_str()).str());

  const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
  if (decoder == nullptr) { return nullptr; }
  if (!decoder->IsOpen()) { return nullptr; }

  int video_stream_index = videoStreamIndexPlug()->getValue();
  if (video_stream_index < 0) {
    video_stream_index = decoder->BestVideoStreamIndex();
    if (video_stream_index < 0) { return nullptr; }
  }

  FrameCacheKey key{};
  key.decoder = decoder.get();
  key.video_stream_index = video_stream_index;
  key.frame_nb = static_cast<int>(context->getFrame());
  FrameCacheEntry frameEntry = frameCache()->get(key);
  if (frameEntry.frame == nullptr) {
    throw IECore::Exception(frameEntry.error != nullptr ? frameEntry.error->c_str() : "");
  }

  TRACE("AvReader", "_retrieveFrame: return frame");
  return frameEntry.frame;
#endif
  const auto *p = videoStreamIndexPlug();
  if (p == nullptr) { return nullptr; }
  if (context == nullptr) { return nullptr; }
  auto *fc = frameCache();
  if (fc == nullptr) { return nullptr; }

  FrameCacheKey key{};
  key.decoder = nullptr;//decoder.get();
  key.video_stream_index = p->getValue();
  key.frame_nb = static_cast<int>(context->getFrame());
  FrameCacheEntry frameEntry = fc->get(key);
  return nullptr;

#if 0
  MissingFrameMode mode = (MissingFrameMode)missingFrameModePlug()->getValue();
  if (holdForBlack && mode == Black) {
    // For some outputs, like "format", we need to hold the value of an adjacent frame when we're
    // going to return black pixels
    mode = Hold;
  }
  ImageReader::ChannelInterpretation channelNaming =
    (ImageReader::ChannelInterpretation)channelInterpretationPlug()->getValue();

  const std::string resolvedFileName = context->substitute(fileName);
#endif

#if 0
  if (!cacheEntry.frame) {
    if (mode == OpenImageIOReader::Black) {
      // we can simply return nullptr and rely on the
      // compute methods to return default plug values.
      return nullptr;
    } else if (mode == OpenImageIOReader::Hold) {
      ConstIntVectorDataPtr frameData = availableFramesPlug()->getValue();
      const std::vector<int> &frames = frameData->readable();
      if (frames.size()) {
        std::vector<int>::const_iterator fIt =
          std::lower_bound(frames.begin(), frames.end(), (int)context->getFrame());

        // decrement to get the previous frame, unless
        // this is the first frame, in which case we
        // hold to the beginning of the sequence
        if (fIt != frames.begin()) { fIt--; }

        // setup a context with the new frame
        Context::EditableScope holdScope(context);
        holdScope.setFrame(*fIt);

        const std::string resolvedFileNameHeld = holdScope.context()->substitute(fileName);
        cacheEntry = cache->get(std::make_pair(resolvedFileNameHeld, channelNaming));
      }

      // if we got here, there was no suitable file sequence, or we weren't able to open the held
      // frame
      if (!cacheEntry.file) { throw IECore::Exception(*(cacheEntry.error)); }
    } else {
      throw IECore::Exception(*(cacheEntry.error));
    }
  }
#endif
}

void AvReader::_plugSet(Gaffer::Plug *plug)
{
  // This clears the cache every time the refresh
  // count is updated, so you don't get entries from
  // old files hanging around.
  if (plug == refreshCountPlug()) {
    TRACE("AvReader", "refreshCount");

    // NOTE(tohi): Clears decoders and frames for ALL AvReader nodes...
    frameCache()->clear();
    shared_decoders_internal::SharedDecoders::clear();
  }
}

}// namespace IlpGafferMovie
