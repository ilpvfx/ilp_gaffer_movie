#include <ilp_gaffer_movie/av_reader.hpp>

// The nested TaskMutex needs to be the first to include tbb
#include <internal/LRUCache.h>

#include <memory>
#include <string>

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <GafferImage/FormatPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImageReader.h>

#include <Gaffer/Context.h>
#include <Gaffer/StringPlug.h>

#include <boost/bind/bind.hpp>

#include <ilp_movie/demux.hpp>
#include <ilp_movie/log.hpp>

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::AvReader);

static const IECore::InternedString kTileBatchIndexContextName("__tileBatchIndex");

namespace {

class Frame
{
public:
  explicit Frame(std::unique_ptr<ilp_movie::DemuxFrame> frame) : _frame(std::move(frame)) {}

#if 0
  // Read a chunk of data from the file, formatted as a tile batch that will be stored on the tile
  // batch plug
  IECore::ConstObjectVectorPtr readTileBatch(const Gaffer::Context *c, Imath::V3i tileBatchIndex) {}

  int readRegion(int subImage,
    const Box2i &targetRegion,
    std::vector<float> &data,
    DeepData &deepData,
    Box2i &exrDataRegion);


private:
#endif
  std::unique_ptr<ilp_movie::DemuxFrame> _frame;
};

// For success, frame should be set, and error left null.
// For failure, frame should be left null, and error should be set.
struct CacheEntry
{
  std::shared_ptr<Frame> frame;
  std::shared_ptr<std::string> error;
};

#if 0
struct CacheKey {
  std::string filename = "";
  int frame = -1;
  ilp_movie::DemuxContext *demux_ctx = nullptr;
}
#endif

CacheEntry FrameCacheGetter(const std::pair<std::string, int> &key,
  size_t &cost,
  const IECore::Canceller * /*canceller*/)
{
  IECore::msg(IECore::Msg::Info, "AvReader", "FrameCacheGetter");

  cost = 1;

  CacheEntry result = {};

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
    if (!str.empty() && str[str.length() - 1] == '\n') { str.erase(str.length() - 1); }
    IECore::msg(iec_level, "MovieReader", str);
  });

  ilp_movie::DemuxContext demux_ctx = {};
  demux_ctx.filename = key.first.c_str();
  if (!ilp_movie::DemuxInit(&demux_ctx)) {
    result.error.reset(new std::string("AvReader : Cannot initialize demuxer"));// NOLINT
    // No need to free the demux context when init fails.
    return result;
  }

  ilp_movie::DemuxFrame demux_frame = {};
  if (!ilp_movie::DemuxSeek(demux_ctx, key.second, &demux_frame)) {
    ilp_movie::DemuxFree(&demux_ctx);
    result.error.reset(new std::string("AvReader : Cannot seek to frame"));// NOLINT
    return result;
  }

  ilp_movie::DemuxFree(&demux_ctx);

  auto demux_frame_ptr = std::make_unique<ilp_movie::DemuxFrame>();
  demux_frame_ptr->width = demux_frame.width;
  demux_frame_ptr->height = demux_frame.height;
  demux_frame_ptr->frame_index = demux_frame.frame_index;
  demux_frame_ptr->buf = std::move(demux_frame.buf);

  result.frame.reset(new Frame(std::move(demux_frame_ptr)));// NOLINT
  return result;

#if 0
  const std::string &fileName = fileNameAndFrame.first;

  std::unique_ptr<ImageInput> imageInput(ImageInput::create(fileName));
  if (!imageInput) {
    result.error.reset(
      new std::string("OpenImageIOReader : Could not create ImageInput : " + OIIO::geterror()));
    return result;
  }

  ImageSpec firstPartSpec;
  if (!imageInput->open(fileName, firstPartSpec)) {
    result.error.reset(
      new std::string("OpenImageIOReader : Could not open ImageInput : " + imageInput->geterror()));
    return result;
  }

  result.file.reset(
    new File(std::move(imageInput), fileName, fileNameAndChannelInterpretation.second));
#endif
}

using FrameCache = IECorePreview::LRUCache<std::pair<std::string, int>, CacheEntry>;

FrameCache *GetFrameCache()
{
  static auto *c = new FrameCache(FrameCacheGetter, 200);
  return c;
}

}// namespace

namespace IlpGafferMovie {

std::size_t AvReader::FirstPlugIndex = 0;

AvReader::AvReader(const std::string &name) : GafferImage::ImageNode(name)
{
  using Plug = Gaffer::Plug;

  storeIndexOfNextChild(FirstPlugIndex);
  addChild(new Gaffer::StringPlug("fileName",
    Plug::In,
    "",
    /*flags=*/Plug::Default)
#if 0
    ,
    /*substitutions=*/IECore::StringAlgo::AllSubstitutions
      & ~IECore::StringAlgo::FrameSubstitutions)
#endif
  );
  addChild(new Gaffer::IntPlug("refreshCount"));
#if 0
  addChild(new IntPlug("missingFrameMode",
    Plug::In,
    /*defaultValue=*/static_cast<int>(MissingFrameMode::kError),
    /*minValue=*/static_cast<int>(MissingFrameMode::kError),
    /*maxValue=*/static_cast<int>(MissingFrameMode::kHold)));
#endif
  addChild(new Gaffer::IntVectorDataPlug("availableFrames", Plug::Out, new IECore::IntVectorData));
  addChild(new Gaffer::ObjectVectorPlug("__tileBatch", Plug::Out, new IECore::ObjectVector));

  // NOLINTNEXTLINE
  plugSetSignal().connect(boost::bind(&AvReader::_plugSet, this, boost::placeholders::_1));
}

Gaffer::StringPlug *AvReader::fileNamePlug()
{
  constexpr size_t kPlugIndex = 0;
  return getChild<Gaffer::StringPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::StringPlug *AvReader::fileNamePlug() const
{
  constexpr size_t kPlugIndex = 0;
  return getChild<Gaffer::StringPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::IntPlug *AvReader::refreshCountPlug()
{
  constexpr size_t kPlugIndex = 1;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntPlug *AvReader::refreshCountPlug() const
{
  constexpr size_t kPlugIndex = 1;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

#if 0
Gaffer::IntPlug *AvReader::missingFrameModePlug()
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntPlug *AvReader::missingFrameModePlug() const
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}
#endif

Gaffer::IntVectorDataPlug *AvReader::availableFramesPlug()
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::IntVectorDataPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntVectorDataPlug *AvReader::availableFramesPlug() const
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::IntVectorDataPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::ObjectVectorPlug *AvReader::_tileBatchPlug()
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::ObjectVectorPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::ObjectVectorPlug *AvReader::_tileBatchPlug() const
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::ObjectVectorPlug>(FirstPlugIndex + kPlugIndex);
}

void AvReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  // IECore::msg(IECore::Msg::Info, "AvReader", "affects");

  GafferImage::ImageNode::affects(input, outputs);

  if (input == fileNamePlug() || input == refreshCountPlug()) {
    outputs.push_back(availableFramesPlug());
  }

  if (input == fileNamePlug() || input == refreshCountPlug()
    //|| input == missingFrameModePlug()
  ) {
    outputs.push_back(_tileBatchPlug());
  }

  if (input == fileNamePlug() || input == refreshCountPlug()
    //|| input == missingFrameModePlug()
  ) {
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

  if (output == availableFramesPlug()) {
    fileNamePlug()->hash(h);
    refreshCountPlug()->hash(h);
  } else if (output == _tileBatchPlug()) {
    h.append(context->get<Imath::V3i>(kTileBatchIndexContextName));
    h.append(context->get<std::string>(
      GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));

    Gaffer::Context::EditableScope c(context);
    c.remove(kTileBatchIndexContextName);

    fileNamePlug()->hash(h);
    h.append(context->getFrame());
    refreshCountPlug()->hash(h);
    // missingFrameModePlug()->hash(h);
  }
}

void AvReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  if (output == availableFramesPlug()) {
    IECore::msg(IECore::Msg::Info, "AvReader", "compute - availableFrames");
#if 0
    FileSequencePtr fileSequence = nullptr;
    IECore::ls(fileNamePlug()->getValue(), fileSequence, /* minSequenceSize */ 1);

    if (fileSequence) {
      IntVectorDataPtr resultData = new IntVectorData;
      std::vector<FrameList::Frame> frames;
      fileSequence->getFrameList()->asList(frames);
      std::vector<int> &result = resultData->writable();
      result.resize(frames.size());
      std::copy(frames.begin(), frames.end(), result.begin());
      static_cast<IntVectorDataPlug *>(output)->setValue(resultData);
    } else {
      static_cast<IntVectorDataPlug *>(output)->setToDefault();
    }
#endif

#if 1
    IECore::IntVectorDataPtr resultData = new IECore::IntVectorData;
    auto &result = resultData->writable();
    result.resize(30);
    int j = 3;
    for (auto &&i : result) { i = j++; }
#endif
  }
#if 0
  else if (output == _tileBatchPlug()) {
    IECore::msg(IECore::Msg::Info, "AvReader", "compute - tileBatch");
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
    IECore::msg(IECore::Msg::Info, "AvReader", "compute - <other>");
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
  GafferImage::ImageNode::hashViewNames(parent, context, h);
  fileNamePlug()->hash(h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
}

IECore::ConstStringVectorDataPtr AvReader::computeViewNames(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  return GafferImage::ImagePlug::defaultViewNames();
  // FilePtr file = std::static_pointer_cast<File>(retrieveFile(context));
  // if (!file) { return ImagePlug::defaultViewNames(); }
  // return file->viewNamesData();
}

void AvReader::hashFormat(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hashFormat(parent, context, h);
  fileNamePlug()->hash(h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(h);
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
  std::shared_ptr<Frame> frame = std::static_pointer_cast<Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return GafferImage::FormatPlug::getDefaultFormat(context); }

  return GafferImage::Format(
    /*displayWindow=*/Imath::Box2i(
      /*minT=*/Imath::V2i(0, 0),
      /*maxT=*/Imath::V2i(frame->_frame->width, frame->_frame->height)),
    /*pixelAspect=*/1.0,
    /*fromEXRSpace=*/false);
}

void AvReader::hashDataWindow(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hashDataWindow(parent, context, h);
  fileNamePlug()->hash(h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

Imath::Box2i AvReader::computeDataWindow(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info, "AvReader", "computeDataWindow");

  std::shared_ptr<Frame> frame = std::static_pointer_cast<Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->dataWindowPlug()->defaultValue(); }
  return Imath::Box2i(
    /*minT=*/Imath::V2i(0, 0), /*maxT=*/Imath::V2i(frame->_frame->width, frame->_frame->height));
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
  IECore::msg(IECore::Msg::Info, "AvReader", "computeMetadata");

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
  fileNamePlug()->hash(h);
  h.append(context->getFrame());
  refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
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
  IECore::msg(IECore::Msg::Info, "AvReader", "computeChannelNames");

  std::shared_ptr<Frame> frame = std::static_pointer_cast<Frame>(_retrieveFrame(context));
  if (frame == nullptr) { return parent->channelNamesPlug()->defaultValue(); }

  // If we have a frame assume that it has RGB channels.
  IECore::StringVectorDataPtr channelNamesData = new IECore::StringVectorData({
    GafferImage::ImageAlgo::channelNameR,
    GafferImage::ImageAlgo::channelNameG,
    GafferImage::ImageAlgo::channelNameB,
  });
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
    // missingFrameModePlug()->hash(h);
  }
}

IECore::ConstFloatVectorDataPtr AvReader::computeChannelData(const std::string &channelName,
  const Imath::V2i &tileOrigin,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info,
    "AvReader",
    boost::format("computeChannelData - channelName = %1%, tileOrigin = (%2%, %3%)") % channelName
      % tileOrigin.x % tileOrigin.y);

  GafferImage::ImagePlug::GlobalScope c(context);
  std::shared_ptr<Frame> frame = std::static_pointer_cast<Frame>(_retrieveFrame(context));

  if (frame == nullptr) { return parent->channelDataPlug()->defaultValue(); }
  IECore::msg(
    IECore::Msg::Info, "AvReader", boost::format("computeChannelData - channelName = got frame"));

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

  const auto *f = frame->_frame.get();
  ilp_movie::PixelData ch = {};
  if (channelName == GafferImage::ImageAlgo::channelNameR) {
    ch = ilp_movie::ChannelData(*f, ilp_movie::Channel::kRed);
  } else if (channelName == GafferImage::ImageAlgo::channelNameG) {
    ch = ilp_movie::ChannelData(*f, ilp_movie::Channel::kGreen);
  } else {// channelName == GafferImage::ImageAlgo::channelNameB
    ch = ilp_movie::ChannelData(*f, ilp_movie::Channel::kBlue);
  }

  const Imath::Box2i tileRegion = GafferImage::BufferAlgo::intersection(tileBound, dataWindow);

  for (int y = tileRegion.min.y; y < tileRegion.max.y; ++y) {
    float *dst = &tile[static_cast<size_t>(y - tileRegion.min.y)
                       * static_cast<size_t>(GafferImage::ImagePlug::tileSize())];
    const float *src = &(ch.data[static_cast<size_t>(y) * static_cast<size_t>(f->width)// NOLINT
                                 + static_cast<size_t>(tileRegion.min.x)]);
    std::memcpy(
      dst, src, sizeof(float) * (static_cast<size_t>(tileRegion.max.x - tileRegion.min.x)));
  }

  return tileData;
}

std::shared_ptr<void> AvReader::_retrieveFrame(const Gaffer::Context *context/*,
  bool holdForBlack = false*/) const
{
  IECore::msg(IECore::Msg::Info,
    "AvReader",
    boost::format("_retrieveFrame: '%1%'") % fileNamePlug()->getValue().c_str());


  const std::string fileName = fileNamePlug()->getValue();
  if (fileName.empty()) { return nullptr; }

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

  const auto key = std::make_pair(fileName, static_cast<int>(context->getFrame()));
  auto *cache = GetFrameCache();
  CacheEntry cacheEntry = cache->get(key);
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

  IECore::msg(IECore::Msg::Info, "AvReader", "_retrieveFrame: return frame");

  return cacheEntry.frame;
}


void AvReader::_plugSet(Gaffer::Plug *plug)
{
  // This clears the cache every time the refresh
  // count is updated, so you don't get entries from
  // old files hanging around.
  if (plug == refreshCountPlug()) {
    // fileCache()->clear();
    GetFrameCache()->clear();
  }
}

}// namespace IlpGafferMovie
