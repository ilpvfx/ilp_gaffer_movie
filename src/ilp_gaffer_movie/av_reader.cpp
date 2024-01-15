#include "ilp_gaffer_movie/av_reader.hpp"

// The nested TaskMutex needs to be the first to include tbb
#include "internal/LRUCache.h"
#include "internal/SharedDecoders.h"
#include "internal/SharedFrames.h"
#include "internal/trace.hpp"

#include <cassert>// assert
#include <mutex>// std::call_once
#include <numeric>// std::iota
#include <string>// std::string
#include <string_view>// std::string_view

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <GafferImage/FormatPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImageReader.h>

#include <Gaffer/Context.h>
#include <Gaffer/StringPlug.h>

#include <boost/bind/bind.hpp>

#include "ilp_movie/decoder.hpp"
#include "ilp_movie/frame.hpp"
#include "ilp_movie/log.hpp"

using namespace std::literals;

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::AvReader);

namespace IlpGafferMovie {

std::size_t AvReader::g_firstPlugIndex = 0;

AvReader::AvReader(const std::string &name) : GafferImage::ImageNode{ name }
{
  using Plug = Gaffer::Plug;
  using BoolPlug = Gaffer::BoolPlug;
  using IntPlug = Gaffer::IntPlug;
  using IntVectorDataPlug = Gaffer::IntVectorDataPlug;
  using IntVectorData = IECore::IntVectorData;
  using StringPlug = Gaffer::StringPlug;

  storeIndexOfNextChild(g_firstPlugIndex);

  addChild(new StringPlug(// [0]
    /*name=*/"fileName",
    /*direction=*/Plug::In));
  addChild(new IntPlug(// [1]
    /*name=*/"refreshCount",
    /*direction=*/Plug::In));
  addChild(new IntPlug(// [2]
    /*name=*/"missingFrameMode",
    /*direction=*/Plug::In,
    /*defaultValue=*/static_cast<int>(MissingFrameMode::Error),
    /*minValue=*/static_cast<int>(MissingFrameMode::Error),
    /*maxValue=*/static_cast<int>(MissingFrameMode::Hold)));
  addChild(new StringPlug(// [3]
    /*name=*/"videoStream",
    /*direction=*/Plug::In));
  addChild(new StringPlug(// [4]
    /*name=*/"filterGraph",
    /*direction=*/Plug::In));

  addChild(new IntVectorDataPlug(// [5]
    /*name=*/"availableFrames",
    /*direction=*/Plug::Out,
    /*defaultValue=*/new IntVectorData));
  addChild(new BoolPlug(// [6]
    /*name=*/"fileValid",
    /*direction=*/Plug::Out));
  addChild(new StringPlug(// [7]
    /*name=*/"probe",
    /*direction=*/Plug::Out));

  // NOLINTNEXTLINE
  plugSetSignal().connect(boost::bind(&AvReader::_plugSet, this, boost::placeholders::_1));
}

// clang-format off
// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL(name, type, index)            \
  type *AvReader::name()                               \
  {                                                    \
    return getChild<type>(g_firstPlugIndex + (index)); \
  }                                                    \
  const type *AvReader::name() const                   \
  {                                                    \
    return getChild<type>(g_firstPlugIndex + (index)); \
  }
// clang-format on

PLUG_MEMBER_IMPL(fileNamePlug, Gaffer::StringPlug, 0U);
PLUG_MEMBER_IMPL(refreshCountPlug, Gaffer::IntPlug, 1U);
PLUG_MEMBER_IMPL(missingFrameModePlug, Gaffer::IntPlug, 2U);
PLUG_MEMBER_IMPL(videoStreamPlug, Gaffer::StringPlug, 3U);
PLUG_MEMBER_IMPL(filterGraphPlug, Gaffer::StringPlug, 4U);

PLUG_MEMBER_IMPL(availableFramesPlug, Gaffer::IntVectorDataPlug, 5U);
PLUG_MEMBER_IMPL(fileValidPlug, Gaffer::BoolPlug, 6U);
PLUG_MEMBER_IMPL(probePlug, Gaffer::StringPlug, 7U);

size_t AvReader::supportedExtensions(std::vector<std::string> &extensions)
{
  extensions = ilp_movie::Decoder::SupportedFormats();
  return extensions.size();
}

void AvReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  // clang-format off

  GafferImage::ImageNode::affects(input, outputs);

  if (input == fileNamePlug() || 
      input == refreshCountPlug()) {
    outputs.push_back(fileValidPlug());
    outputs.push_back(probePlug());
  }

  if (input == fileNamePlug() || 
      input == refreshCountPlug() ||
      input == videoStreamPlug()) {
    outputs.push_back(availableFramesPlug());
  }

  if (input == fileNamePlug() || 
      input == refreshCountPlug() || 
      input == missingFrameModePlug() || 
      input == videoStreamPlug() ||
      input == filterGraphPlug()) {
    for (Gaffer::ValuePlug::Iterator it(outPlug()); !it.done(); ++it) {
      outputs.push_back(it->get());
    }
  }

  // clang-format on
}

void AvReader::hash(const Gaffer::ValuePlug *output,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  // clang-format off

  GafferImage::ImageNode::hash(output, context, /*out*/ h);

  if (output == fileValidPlug() || 
      output == probePlug()) {
    fileNamePlug()->hash(/*out*/ h);
    refreshCountPlug()->hash(/*out*/ h);
  } else if (output == availableFramesPlug()) {
    fileNamePlug()->hash(/*out*/ h);
    refreshCountPlug()->hash(/*out*/ h);
    videoStreamPlug()->hash(/*out*/ h);
  }

  // clang-format on
}

void AvReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  using BoolPlug = Gaffer::BoolPlug;
  using IntVectorData = IECore::IntVectorData;
  using IntVectorDataPtr = IECore::IntVectorDataPtr;
  using IntVectorDataPlug = Gaffer::IntVectorDataPlug;
  using StringPlug = Gaffer::StringPlug;

  if (output == fileValidPlug()) {
    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    if (decoder == nullptr) {
      static_cast<BoolPlug *>(output)->setValue(false);// NOLINT
    } else {
      static_cast<BoolPlug *>(output)->setValue(decoder->IsOpen());// NOLINT
    }
  } else if (output == availableFramesPlug()) {
    // Check first to possibly avoid querying the decoder cache.
    const auto idx = _videoStreamIndex(context);
    if (!idx.has_value()) {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
      return;
    }

    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    if (decoder == nullptr) {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
      return;
    }

    const auto hdr = decoder->VideoStreamHeader(*idx);
    if (!hdr.has_value()) {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();// NOLINT
      return;
    }

    IntVectorDataPtr resultData = new IntVectorData;
    auto &result = resultData->writable();
    result.resize(static_cast<std::size_t>(hdr->frame_count));
    std::iota(std::begin(result), std::end(result), hdr->first_frame_nb);
    static_cast<IntVectorDataPlug *>(output)->setValue(resultData);// NOLINT
  } else if (output == probePlug()) {
    const auto decoder = std::static_pointer_cast<ilp_movie::Decoder>(_retrieveDecoder(context));
    if (decoder != nullptr) {
      static_cast<StringPlug *>(output)->setValue(decoder->Probe());// NOLINT
    } else {
      static_cast<StringPlug *>(output)->setToDefault();// NOLINT
    }
  } else {
    ImageNode::compute(output, context);
  }
}

Gaffer::ValuePlug::CachePolicy AvReader::computeCachePolicy(const Gaffer::ValuePlug *output) const
{
  if (output == outPlug()->channelDataPlug()) {
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
  ImageNode::hashFormat(parent, context, /*out*/ h);
  fileNamePlug()->hash(/*out*/ h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(/*out*/ h);
  missingFrameModePlug()->hash(/*out*/ h);
  videoStreamPlug()->hash(/*out*/ h);
  filterGraphPlug()->hash(/*out*/ h);

  // Check if defaults have changed.
  const auto format = GafferImage::FormatPlug::getDefaultFormat(context);
  h.append(format.getDisplayWindow());
  h.append(format.getPixelAspect());
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

GafferImage::Format AvReader::computeFormat(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // NOTE(tohi):
  // We cannot rely on the video stream headers here, since the pixel dimensions of
  // the video frames could be modified by the filter graph.

  const auto frame =
    std::static_pointer_cast<ilp_movie::Frame>(_retrieveFrame(context, /*holdForBlack=*/true));
  if (frame == nullptr) { return GafferImage::FormatPlug::getDefaultFormat(context); }

  double pixelAspect = 1.0;
  if (frame->hdr.pixel_aspect_ratio.num > 0 && frame->hdr.pixel_aspect_ratio.den > 0) {
    pixelAspect =
      static_cast<double>(frame->hdr.pixel_aspect_ratio.num) / frame->hdr.pixel_aspect_ratio.den;
  }

  // clang-format off
  return GafferImage::Format{ 
    Imath::Box2i{ 
      /*minT=*/Imath::V2i{ 0, 0 },
      /*maxT=*/Imath::V2i{ frame->hdr.width, frame->hdr.height } },
    pixelAspect };
  // clang-format on
}

void AvReader::hashDataWindow(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hashDataWindow(parent, context, /*out*/ h);
  fileNamePlug()->hash(/*out*/ h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(/*out*/ h);
  missingFrameModePlug()->hash(/*out*/ h);
  videoStreamPlug()->hash(/*out*/ h);
  filterGraphPlug()->hash(/*out*/ h);

  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

Imath::Box2i AvReader::computeDataWindow(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  // NOTE(tohi):
  // We cannot rely on the video stream headers here, since the pixel dimensions of
  // the video frames could be modified by the filter graph.

  const auto frame = std::static_pointer_cast<ilp_movie::Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->dataWindowPlug()->defaultValue(); }

  // clang-format off
  return Imath::Box2i{ 
      /*minT=*/Imath::V2i{ 0, 0 },
      /*maxT=*/Imath::V2i{ frame->hdr.width, frame->hdr.height } };
  // clang-format on
}

void AvReader::hashMetadata(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashMetadata(parent, context, /*out*/ h);
  fileNamePlug()->hash(/*out*/ h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(/*out*/ h);
  missingFrameModePlug()->hash(/*out*/ h);
  videoStreamPlug()->hash(/*out*/ h);
  filterGraphPlug()->hash(/*out*/ h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstCompoundDataPtr AvReader::computeMetadata(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  const auto frame = std::static_pointer_cast<ilp_movie::Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->metadataPlug()->defaultValue(); }

  // In theory we could use metadata, like written below, to perform
  // "optimal" color conversion using OCIO. In practice, however, we
  // fallback on heuristics, but if we were to try a more sophisticated
  // approach it is likely we would need metadata regarding the "FFmpeg colorspace"
  // of the frame.

  // clang-format off
  IECore::CompoundDataPtr result = new IECore::CompoundData;
  result->writable()["pixelFormat"] = new IECore::StringData(frame->hdr.pix_fmt_name);// NOLINT
  result->writable()["colorRange"] = new IECore::StringData(frame->hdr.color_range_name);// NOLINT
  result->writable()["colorSpace"] = new IECore::StringData(frame->hdr.color_space_name);// NOLINT
  result->writable()["colorTrc"] = new IECore::StringData(frame->hdr.color_trc_name);// NOLINT
  result->writable()["colorPrimaries"] = new IECore::StringData(frame->hdr.color_primaries_name);// NOLINT
  // clang-format on

  return result;
}

void AvReader::hashDeep(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashDeep(parent, context, /*out*/ h);
}

bool AvReader::computeDeep(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // Always false for video, this is an EXR thing!?
  return false;
}

void AvReader::hashSampleOffsets(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  using ImagePlug = GafferImage::ImagePlug;

  GafferImage::ImageNode::hashSampleOffsets(parent, context, /*out*/ h);

  h.append(context->get<Imath::V2i>(ImagePlug::tileOriginContextName));
  h.append(context->get<std::string>(ImagePlug::viewNameContextName, ImagePlug::defaultViewName));

  {
    ImagePlug::GlobalScope c(context);
    fileNamePlug()->hash(/*out*/ h);
    h.append(context->getFrame());
    refreshCountPlug()->hash(/*out*/ h);
    missingFrameModePlug()->hash(/*out*/ h);
    videoStreamPlug()->hash(/*out*/ h);
  }
}

IECore::ConstIntVectorDataPtr AvReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // We don't have to worry about "deep" images or tile batches since we are not dealing
  // with EXR images.
  return GafferImage::ImagePlug::flatTileSampleOffsets();
}

void AvReader::hashChannelNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashChannelNames(parent, context, /*out*/ h);
  fileNamePlug()->hash(/*out*/ h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(/*out*/ h);
  missingFrameModePlug()->hash(/*out*/ h);
  videoStreamPlug()->hash(/*out*/ h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstStringVectorDataPtr AvReader::computeChannelNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  auto frame = std::static_pointer_cast<ilp_movie::Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->channelNamesPlug()->defaultValue(); }

  // Check if we can access channel data for the frame to determine which
  // channels to request later.

  std::vector<std::string> channelNames;

  using ilp_movie::CompPixelData;
  namespace Comp = ilp_movie::Comp;
  if (!Empty(CompPixelData<const float>(
        frame->data, frame->linesize, Comp::kR, frame->hdr.height, frame->hdr.pix_fmt_name))) {
    channelNames.push_back(GafferImage::ImageAlgo::channelNameR);
  }
  if (!Empty(CompPixelData<const float>(
        frame->data, frame->linesize, Comp::kG, frame->hdr.height, frame->hdr.pix_fmt_name))) {
    channelNames.push_back(GafferImage::ImageAlgo::channelNameG);
  }
  if (!Empty(CompPixelData<const float>(
        frame->data, frame->linesize, Comp::kB, frame->hdr.height, frame->hdr.pix_fmt_name))) {
    channelNames.push_back(GafferImage::ImageAlgo::channelNameB);
  }
  if (!Empty(CompPixelData<const float>(
        frame->data, frame->linesize, Comp::kA, frame->hdr.height, frame->hdr.pix_fmt_name))) {
    channelNames.push_back(GafferImage::ImageAlgo::channelNameA);
  }

  return IECore::StringVectorDataPtr{ new IECore::StringVectorData{ // NOLINT
    std::move(channelNames) } };
}

void AvReader::hashChannelData(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashChannelData(parent, context, /*out*/ h);
  h.append(context->get<Imath::V2i>(GafferImage::ImagePlug::tileOriginContextName));
  h.append(context->get<std::string>(GafferImage::ImagePlug::channelNameContextName));
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));

  {
    GafferImage::ImagePlug::GlobalScope c(context);
    fileNamePlug()->hash(/*out*/ h);
    h.append(context->getFrame());
    refreshCountPlug()->hash(/*out*/ h);
    missingFrameModePlug()->hash(/*out*/ h);
    videoStreamPlug()->hash(/*out*/ h);
    filterGraphPlug()->hash(/*out*/ h);
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

  GafferImage::ImagePlug::GlobalScope globalScope(context);
  auto frame = std::static_pointer_cast<ilp_movie::Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->channelDataPlug()->defaultValue(); }

  // Determine which component/channel we want to access.
  namespace Comp = ilp_movie::Comp;
  Comp::ValueType c = Comp::kUnknown;
  if (channelName == GafferImage::ImageAlgo::channelNameR) {
    c = ilp_movie::Comp::kR;
  } else if (channelName == GafferImage::ImageAlgo::channelNameG) {
    c = ilp_movie::Comp::kG;
  } else if (channelName == GafferImage::ImageAlgo::channelNameB) {
    c = ilp_movie::Comp::kB;
  } else if (channelName == GafferImage::ImageAlgo::channelNameA) {
    c = ilp_movie::Comp::kA;
  } else {
    throw IECore::Exception("Unexpected channel name");
  }

  using ilp_movie::CompPixelData;
  const auto pix = CompPixelData<const float>(
    frame->data, frame->linesize, c, frame->hdr.height, frame->hdr.pix_fmt_name);
  if (Empty(pix)) { throw IECore::Exception("Empty pixel data"); }

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

  IECore::FloatVectorDataPtr tileData = new IECore::FloatVectorData(
    std::vector<float>(static_cast<size_t>(GafferImage::ImagePlug::tilePixels())));
  auto &tile = tileData->writable();

  const Imath::Box2i tileRegion = GafferImage::BufferAlgo::intersection(tileBound, dataWindow);

  constexpr auto kTileSize = static_cast<size_t>(GafferImage::ImagePlug::tileSize());
  for (int y = tileRegion.min.y; y < tileRegion.max.y; ++y) {
    float *dst = &tile[static_cast<size_t>(y - tileRegion.min.y) * kTileSize];
    const float *src =
      &(pix.data[static_cast<size_t>(y) * static_cast<size_t>(frame->hdr.width)// NOLINT
                 + static_cast<size_t>(tileRegion.min.x)]);
    std::memcpy(dst, src, sizeof(float) * static_cast<size_t>(tileRegion.max.x - tileRegion.min.x));
  }

  return tileData;
}

std::optional<int> AvReader::_videoStreamIndex(const Gaffer::Context *context) const
{
  std::optional<int> idx{};
  const std::string str = context->substitute(videoStreamPlug()->getValue());
  if (str == "best") {
    // This special value is used to communicate to the decoder to use the "best"
    // video stream, as determined by the decoder itself.
    idx = -1;
  } else {
    int p{};
    if (std::from_chars(str.data(), str.data() + str.size(), /*out*/ p).ec// NOLINT
        == std::errc()) {
      idx = p;
    }
  }
  return idx;
}

std::string AvReader::_filterGraph(const Gaffer::Context *context) const
{
  std::string resolvedFilterGraph = context->substitute(filterGraphPlug()->getValue());
  if (resolvedFilterGraph.empty()) {
    // Pass-through video filter.
    resolvedFilterGraph = "null";
  }
  return resolvedFilterGraph;
}

std::shared_ptr<void> AvReader::_retrieveDecoder(const Gaffer::Context *context) const
{
  const std::string fileName = fileNamePlug()->getValue();
  if (fileName.empty()) { return nullptr; }
  std::string resolvedFileName = context->substitute(fileName);

  // TODO(tohi): We are never including alpha values here. We 
  //             would need a way to check if this is needed.

  static const std::string kPixFmtName = "gbrpf32le";

  // clang-format off
  const auto decoderEntry = shared_decoders_internal::SharedDecoders::get(
    /*key=*/shared_decoders_internal::DecoderCacheKey{ 
      /*.fileName=*/std::move(resolvedFileName),
      /*.filterGraphDescr=*/{ 
        /*.filter_descr=*/_filterGraph(context),
        /*.out_pix_fmt_name=*/kPixFmtName
      } 
    });
  // clang-format on

  if (decoderEntry.decoder == nullptr) {
    throw IECore::Exception(decoderEntry.error != nullptr ? decoderEntry.error->c_str() : "");
  }

  return decoderEntry.decoder;
}

std::shared_ptr<void> AvReader::_retrieveFrame(const Gaffer::Context *context,
  const bool holdForBlack) const
{
  const std::string fileName = fileNamePlug()->getValue();
  if (fileName.empty()) { return nullptr; }

  const auto idx = _videoStreamIndex(context);
  if (!idx.has_value()) { return nullptr; }

  const int frameNb = static_cast<int>(context->getFrame());

  // clang-format off
  auto frameEntry = shared_frames_internal::SharedFrames::get(
    /*key=*/shared_frames_internal::FrameCacheKey{
      /*.decoder_key=*/shared_decoders_internal::DecoderCacheKey{ 
        /*.fileName=*/context->substitute(fileName),
        /*.filterGraphDescr=*/{ 
          /*.filter_descr=*/_filterGraph(context),
          /*.out_pix_fmt_name=*/"gbrpf32le" 
        } 
      },
      /*.video_stream_index=*/*idx,
      /*.frame_nb=*/frameNb
    });
  // clang-format on

  if (frameEntry.frame == nullptr) {
    auto mode = static_cast<AvReader::MissingFrameMode>(missingFrameModePlug()->getValue());
    if (holdForBlack && mode == AvReader::MissingFrameMode::Black) {
      // For some outputs, like "format", we need to hold the value of an
      // adjacent frame when we're going to return black pixels.
      mode = AvReader::MissingFrameMode::Hold;
    }

    if (mode == AvReader::MissingFrameMode::Black) {
      // We can simply return nullptr and rely on the compute
      // methods to return default plug values.
      return nullptr;
    } else if (mode == AvReader::MissingFrameMode::Hold) {
      IECore::ConstIntVectorDataPtr frameData = availableFramesPlug()->getValue();
      auto &&frames = frameData->readable();
      if (!frames.empty()) {
        auto fIt = std::lower_bound(frames.begin(), frames.end(), frameNb);

        // Get the previous frame, unless this is the first frame, in which
        // case we hold to the beginning of the sequence.
        if (fIt != frames.begin()) { fIt = std::prev(fIt); }

        // Setup a context with the new frame.
        Gaffer::Context::EditableScope holdScope(context);
        holdScope.setFrame(static_cast<float>(*fIt));

        // clang-format off
        frameEntry = shared_frames_internal::SharedFrames::get(
          /*key=*/shared_frames_internal::FrameCacheKey{
            /*.decoder_key=*/shared_decoders_internal::DecoderCacheKey{ 
              /*.fileName=*/holdScope.context()->substitute(fileName),
              /*.filterGraphDescr=*/{ 
                /*.filter_descr=*/_filterGraph(holdScope.context()),
                /*.out_pix_fmt_name=*/"gbrpf32le" 
              } 
            },
            /*.video_stream_index=*/*idx,
            /*.frame_nb=*/static_cast<int>(holdScope.context()->getFrame())
          });
        // clang-format on
      }

      // If we got here, there was no suitable decoder, or we weren't able to open the
      // held frame.
      if (!frameEntry.frame) {
        throw IECore::Exception(frameEntry.error != nullptr ? frameEntry.error->c_str() : "");
      }
    } else {
      throw IECore::Exception(frameEntry.error != nullptr ? frameEntry.error->c_str() : "");
    }
  }

  return frameEntry.frame;
}

void AvReader::_plugSet(Gaffer::Plug *plug)
{
  // NOTE(tohi):
  // Potentially clears decoders and frames for ALL AvReader nodes...

  if (plug == refreshCountPlug()) {
    // This clears the cache every time the refresh count is updated, so we don't have
    // entries from old files hanging around.
    shared_frames_internal::SharedFrames::clear();
    shared_decoders_internal::SharedDecoders::clear();
  }

  if (plug == filterGraphPlug()) {
    // This clears the frame cache every time the filter graph is updated, since
    // this may cause frames to be rescaled, i.e. alter the pixel dimensions.
    // Opened decoders are still valid.
    shared_frames_internal::SharedFrames::clear();
  }
}

}// namespace IlpGafferMovie
