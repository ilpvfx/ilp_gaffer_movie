#include <ilp_gaffer_movie/av_reader.hpp>

#include <string>

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include "GafferImage/FormatPlug.h"
#include "GafferImage/ImageAlgo.h"
#include "GafferImage/ImageReader.h"

#include "Gaffer/Context.h"
#include "Gaffer/StringPlug.h"

#include "boost/bind/bind.hpp"

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::AvReader);

static const IECore::InternedString kTileBatchIndexContextName("__tileBatchIndex");

namespace IlpGafferMovie {

std::size_t AvReader::FirstPlugIndex = 0;

AvReader::AvReader(const std::string &name) : GafferImage::ImageNode(name)
{
  using Plug = Gaffer::Plug;
  using IntPlug = Gaffer::IntPlug;

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
  addChild(new IntPlug("refreshCount"));
  addChild(new IntPlug("missingFrameMode",
    Plug::In,
    /*defaultValue=*/static_cast<int>(MissingFrameMode::kError),
    /*minValue=*/static_cast<int>(MissingFrameMode::kError),
    /*maxValue=*/static_cast<int>(MissingFrameMode::kHold)));
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

Gaffer::IntVectorDataPlug *AvReader::availableFramesPlug()
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::IntVectorDataPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntVectorDataPlug *AvReader::availableFramesPlug() const
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::IntVectorDataPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::ObjectVectorPlug *AvReader::_tileBatchPlug()
{
  constexpr size_t kPlugIndex = 4;
  return getChild<Gaffer::ObjectVectorPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::ObjectVectorPlug *AvReader::_tileBatchPlug() const
{
  constexpr size_t kPlugIndex = 4;
  return getChild<Gaffer::ObjectVectorPlug>(FirstPlugIndex + kPlugIndex);
}

void AvReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  IECore::msg(IECore::Msg::Info, "AvReader", "affects");

  GafferImage::ImageNode::affects(input, outputs);

  if (input == fileNamePlug() || input == refreshCountPlug()) {
    outputs.push_back(availableFramesPlug());
  }

  if (input == fileNamePlug() || input == refreshCountPlug() || input == missingFrameModePlug()) {
    outputs.push_back(_tileBatchPlug());
  }

  if (input == fileNamePlug() || input == refreshCountPlug() || input == missingFrameModePlug()) {
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

    // hashFileName(c.context(), h);
    fileNamePlug()->hash(h);
    refreshCountPlug()->hash(h);
    missingFrameModePlug()->hash(h);
  }
}

void AvReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  IECore::msg(IECore::Msg::Info, "AvReader", "compute");

  if (output == availableFramesPlug()) {
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
  } else if (output == _tileBatchPlug()) {
    // const auto tileBatchIndex = context->get<Imath::V3i>(kTileBatchIndexContextName);

    Gaffer::Context::EditableScope c(context);
    c.remove(kTileBatchIndexContextName);

#if 0
    FilePtr file = std::static_pointer_cast<File>(retrieveFile(c.context()));

    if (!file) {
      throw IECore::Exception(
        "OpenImageIOReader - trying to evaluate _tileBatchPlug() with invalid file, this should "
        "never happen.");
    }

    static_cast<ObjectVectorPlug *>(output)->setValue(file->readTileBatch(context, tileBatchIndex));
#endif
  } else {
    ImageNode::compute(output, context);
  }
}


void AvReader::hashViewNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashViewNames(parent, context, h);
  fileNamePlug()->hash(h);
  // hashFileName(context, h);
  refreshCountPlug()->hash(h);
  missingFrameModePlug()->hash(h);
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
  // hashFileName( context, h );
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  missingFrameModePlug()->hash(h);
  const auto format = GafferImage::FormatPlug::getDefaultFormat(context);
  h.append(format.getDisplayWindow());
  h.append(format.getPixelAspect());
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

GafferImage::Format AvReader::computeFormat(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // if (!file) { return FormatPlug::getDefaultFormat(context); }

  return GafferImage::Format(
    /*displayWindow=*/Imath::Box2i(
      /*minT=*/Imath::V2i(0, 0),
      /*maxT=*/Imath::V2i(479, 318)),
    /*pixelAspect=*/1.0,
    /*fromEXRSpace=*/false);

#if 0
  // when we're in MissingFrameMode::Black we still want to
  // match the format of the Hold frame, so pass true for holdForBlack.
  FilePtr file = std::static_pointer_cast<File>(retrieveFile(context, true));
  if (!file) { return FormatPlug::getDefaultFormat(context); }

  const ImageSpec &spec = file->imageSpec(context);
  return GafferImage::Format(
    Imath::Box2i(Imath::V2i(spec.full_x, spec.full_y),
      Imath::V2i(spec.full_x + spec.full_width, spec.full_y + spec.full_height)),
    spec.get_float_attribute("PixelAspectRatio", 1.0f));
#endif
}


void AvReader::hashDataWindow(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hashDataWindow(parent, context, h);
  // hashFileName(context, h);
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

Imath::Box2i AvReader::computeDataWindow(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "AvReader", "computeDataWindow");

  // if (file == nullptr) {
  //   return parent->dataWindowPlug()->defaultValue();
  // }
  const int x = 0;
  const int y = 0;
  const int width = 512;
  const int height = 512;
  return Imath::Box2i(/*minT=*/Imath::V2i(x, y),
    /*maxT=*/Imath::V2i(width + x, height + y));
}

void AvReader::hashMetadata(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashMetadata(parent, context, h);
  //	hashFileName( context, h );
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  missingFrameModePlug()->hash(h);
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

  const auto dataType = std::string{ "uint" };
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
  // hashFileName( context, h );
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

bool AvReader::computeDeep(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  return false;
}

void AvReader::hashSampleOffsets(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashSampleOffsets(parent, context, h);

  h.append(context->get<Imath::V2i>(GafferImage::ImagePlug::tileOriginContextName));
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));

  {
    GafferImage::ImagePlug::GlobalScope c(context);
    // hashFileName(context, h);
    fileNamePlug()->hash(h);
    refreshCountPlug()->hash(h);
    missingFrameModePlug()->hash(h);
  }
}

IECore::ConstIntVectorDataPtr AvReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  return GafferImage::ImagePlug::flatTileSampleOffsets();
}

void AvReader::hashChannelNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashChannelNames(parent, context, h);
  // hashFileName( context, h );
  fileNamePlug()->hash(h);
  refreshCountPlug()->hash(h);
  missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstStringVectorDataPtr AvReader::computeChannelNames(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug *parent) const
{
  return parent->channelNamesPlug()->defaultValue();
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
    // hashFileName(context, h);
    fileNamePlug()->hash(h);
    refreshCountPlug()->hash(h);
    missingFrameModePlug()->hash(h);
  }
}

IECore::ConstFloatVectorDataPtr AvReader::computeChannelData(const std::string & /*channelName*/,
  const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info, "AvReader", "computeChannelData");

  GafferImage::ImagePlug::GlobalScope c(context);
  // if (!file) { return parent->channelDataPlug()->defaultValue(); }
  return parent->channelDataPlug()->defaultValue();

#if 0
  const Imath::Box2i dataWindow = outPlug()->dataWindowPlug()->getValue();
  const Imath::Box2i tileBound(
    tileOrigin, tileOrigin + Imath::V2i(GafferImage::ImagePlug::tileSize()));
  if (!GafferImage::BufferAlgo::intersects(dataWindow, tileBound)) {
    throw IECore::Exception(
      boost::str(boost::format("AvReader : Invalid tile (%i,%i) -> (%i,%i) not within "
                               "data window (%i,%i) -> (%i,%i).")
                 % tileBound.min.x % tileBound.min.y % tileBound.max.x % tileBound.max.y
                 % dataWindow.min.x % dataWindow.min.y % dataWindow.max.x % dataWindow.max.y));
  }

  // ...
#endif
}


void AvReader::_plugSet(Gaffer::Plug *plug)
{
  // This clears the cache every time the refresh count is updated, so you don't get entries
  // from old files hanging around.
  if (plug == refreshCountPlug()) {
    // fileCache()->clear();
  }
}

}// namespace IlpGafferMovie
