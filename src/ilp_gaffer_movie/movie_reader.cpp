
#if 0
// This file will be generated automatically when cur_you run the CMake
// configuration step. It creates a namespace called `myproject`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>
#endif

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <ilp_gaffer_movie/av_reader.hpp>
#include <ilp_gaffer_movie/movie_reader.hpp>

#include <Gaffer/StringPlug.h>

#include <GafferImage/FormatPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImagePlug.h>

// #include "ilp_gaffer_movie_writer/mux.h"

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::MovieReader);

namespace IlpGafferMovie {

size_t MovieReader::FirstPlugIndex = 0U;

MovieReader::MovieReader(const std::string &name) : GafferImage::ImageNode(name)
{
  using Plug = Gaffer::Plug;

  constexpr auto kPlugDefault = static_cast<unsigned int>(Plug::Default);
  constexpr auto kPlugSerializable = static_cast<unsigned int>(Plug::Serialisable);

  storeIndexOfNextChild(FirstPlugIndex);
  // addChild(new ImagePlug("in", Plug::In));
  addChild(new Gaffer::StringPlug("fileName", Plug::In, "", /*flags=*/Plug::Default));
  addChild(new Gaffer::IntPlug("refreshCount"));
  addChild(new Gaffer::IntPlug("missingFrameMode", Plug::In
#if 0
    , Error, /* min */ Error, /* max */ Hold
#endif
    ));

  // addChild(new ImagePlug("out", Plug::Out, plug_default & ~plug_serializable));

  addChild(new Gaffer::IntPlug("channelInterpretation", Plug::In
#if 0
    ,
    (int)ChannelInterpretation::Default,
    /* min */ (int)ChannelInterpretation::Legacy,
    /* max */ (int)ChannelInterpretation::Specification
#endif
    ));

  addChild(new Gaffer::AtomicCompoundDataPlug("__intermediateMetadata",
    Plug::In,
    new IECore::CompoundData,
    kPlugDefault & ~kPlugSerializable));
  // addChild(new Gaffer::StringPlug(
  //   "__intermediateColorSpace", Plug::Out, "", Plug::Default & ~Plug::Serialisable));
  addChild(
    new GafferImage::ImagePlug("__intermediateImage", Plug::In, kPlugDefault & ~kPlugSerializable));

  // We don't really do much work ourselves - we just
  // defer to internal nodes to do the hard work.

  AvReaderPtr avReader = new AvReader("__avReader");
  addChild(avReader);
  avReader->fileNamePlug()->setInput(fileNamePlug());
  avReader->refreshCountPlug()->setInput(refreshCountPlug());
  avReader->missingFrameModePlug()->setInput(missingFrameModePlug());
  avReader->channelInterpretationPlug()->setInput(channelInterpretationPlug());
  _intermediateMetadataPlug()->setInput(avReader->outPlug()->metadataPlug());

  _intermediateImagePlug()->setInput(avReader->outPlug());

  // outPlug()->setInput(inPlug());
}

Gaffer::StringPlug *MovieReader::fileNamePlug()
{
  constexpr size_t kPlugIndex = 0;
  return getChild<Gaffer::StringPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::StringPlug *MovieReader::fileNamePlug() const
{
  constexpr size_t kPlugIndex = 0;
  return getChild<Gaffer::StringPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::IntPlug *MovieReader::refreshCountPlug()
{
  constexpr size_t kPlugIndex = 1;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntPlug *MovieReader::refreshCountPlug() const
{
  constexpr size_t kPlugIndex = 1;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::IntPlug *MovieReader::missingFrameModePlug()
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntPlug *MovieReader::missingFrameModePlug() const
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::IntPlug *MovieReader::channelInterpretationPlug()
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::IntPlug *MovieReader::channelInterpretationPlug() const
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::IntPlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::AtomicCompoundDataPlug *MovieReader::_intermediateMetadataPlug()
{
  constexpr size_t kPlugIndex = 4;
  return getChild<Gaffer::AtomicCompoundDataPlug>(FirstPlugIndex + kPlugIndex);
}
const Gaffer::AtomicCompoundDataPlug *MovieReader::_intermediateMetadataPlug() const
{
  constexpr size_t kPlugIndex = 4;
  return getChild<Gaffer::AtomicCompoundDataPlug>(FirstPlugIndex + kPlugIndex);
}

GafferImage::ImagePlug *MovieReader::_intermediateImagePlug()
{
  constexpr size_t kPlugIndex = 5;
  return getChild<GafferImage::ImagePlug>(FirstPlugIndex + kPlugIndex);
}

const GafferImage::ImagePlug *MovieReader::_intermediateImagePlug() const
{
  constexpr size_t kPlugIndex = 5;
  return getChild<GafferImage::ImagePlug>(FirstPlugIndex + kPlugIndex);
}

AvReader *MovieReader::_avReader()
{
  constexpr size_t kPlugIndex = 6;
  return getChild<AvReader>(FirstPlugIndex + kPlugIndex);
}

const AvReader *MovieReader::_avReader() const
{
  constexpr size_t kPlugIndex = 6;
  return getChild<AvReader>(FirstPlugIndex + kPlugIndex);
}

void MovieReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  ImageNode::affects(input, outputs);

  if (input == _intermediateMetadataPlug() /*|| input == colorSpacePlug()*/) {
    //outputs.push_back(intermediateColorSpacePlug());
  } else if (input->parent<GafferImage::ImagePlug>() == _intermediateImagePlug()) {
    outputs.push_back(outPlug()->getChild<Gaffer::ValuePlug>(input->getName()));
  } /*else if (input == startFramePlug() || input == startModePlug() || input == endFramePlug()
             || input == endModePlug()) {
    for (ValuePlug::Iterator it(outPlug()); !it.done(); ++it) { outputs.push_back(it->get()); }
  }*/
}

void MovieReader::hash(const Gaffer::ValuePlug *output,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hash(output, context, h);

  // if (output == _intermediateColorSpacePlug()) {
  //   _intermediateMetadataPlug()->hash(h);
  //   colorSpacePlug()->hash(h);
  //   fileNamePlug()->hash(h);
  // }
}

void MovieReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  ImageNode::compute(output, context);
}

void MovieReader::hashViewNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashViewNames(parent, context, h);
  // hashFileName( context, h );
  // refreshCountPlug()->hash( h );
  // missingFrameModePlug()->hash( h );
  // channelInterpretationPlug()->hash( h );  // Affects whether "main" is interpreted as "default"
}


IECore::ConstStringVectorDataPtr MovieReader::computeViewNames(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeViewNames");
  return GafferImage::ImagePlug::defaultViewNames();
}

void MovieReader::hashFormat(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashFormat(parent, context, h);
  // hashFileName(context, h);
  // refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  GafferImage::Format format = GafferImage::FormatPlug::getDefaultFormat(context);
  h.append(format.getDisplayWindow());
  h.append(format.getPixelAspect());
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

GafferImage::Format MovieReader::computeFormat(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeFormat");
  // GafferImage::FormatPlug::getDefaultFormat(context);
  return GafferImage::Format(/*width=*/256, /*height=*/256, /*pixelAspect=*/1.0);
}

void MovieReader::hashDataWindow(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashDataWindow(parent, context, h);
  // hashFileName(context, h);
  // refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

Imath::Box2i MovieReader::computeDataWindow(const Gaffer::Context *context,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeDataWindow");
  (void)context;
  (void)parent;
  return {};
}

void MovieReader::hashMetadata(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashMetadata(parent, context, h);
  // hashFileName( context, h );
  // refreshCountPlug()->hash( h );
  // missingFrameModePlug()->hash( h );
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstCompoundDataPtr MovieReader::computeMetadata(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeMetadata");
  return parent->metadataPlug()->defaultValue();
}

void MovieReader::hashDeep(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashDeep(parent, context, h);
  // hashFileName(context, h);
  // refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

bool MovieReader::computeDeep(const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeDeep");
  return false;
}

void MovieReader::hashSampleOffsets(const GafferImage::ImagePlug *parent,
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
    // refreshCountPlug()->hash(h);
    // missingFrameModePlug()->hash(h);
    // channelInterpretationPlug()->hash(h);
  }
}

IECore::ConstIntVectorDataPtr MovieReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeSampleOffsets");
  return GafferImage::ImagePlug::flatTileSampleOffsets();
}

void MovieReader::hashChannelNames(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  GafferImage::ImageNode::hashChannelNames(parent, context, h);
  // hashFileName(context, h);
  // refreshCountPlug()->hash(h);
  // missingFrameModePlug()->hash(h);
  // channelInterpretationPlug()->hash(h);
  h.append(context->get<std::string>(
    GafferImage::ImagePlug::viewNameContextName, GafferImage::ImagePlug::defaultViewName));
}

IECore::ConstStringVectorDataPtr MovieReader::computeChannelNames(
  const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeChannelNames");
  return parent->channelNamesPlug()->defaultValue();
}

void MovieReader::hashChannelData(const GafferImage::ImagePlug *parent,
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
    // hashFileName( context, h );
    // refreshCountPlug()->hash( h );
    // missingFrameModePlug()->hash( h );
    // channelInterpretationPlug()->hash( h );
  }
}

IECore::ConstFloatVectorDataPtr MovieReader::computeChannelData(const std::string & /*channelName*/,
  const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context * /*context*/,
  const GafferImage::ImagePlug *parent) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeChannelData");
  return parent->channelDataPlug()->defaultValue();
}

}// namespace IlpGafferMovie