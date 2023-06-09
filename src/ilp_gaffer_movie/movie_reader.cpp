
#if 0
// This file will be generated automatically when cur_you run the CMake
// configuration step. It creates a namespace called `myproject`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>
#endif

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <ilp_gaffer_movie/movie_reader.hpp>

#include "GafferImage/FormatPlug.h"
#include <Gaffer/StringPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImagePlug.h>

// #include "ilp_gaffer_movie_writer/mux.h"

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::MovieReader);

// using namespace Gaffer;
//  using namespace GafferImage;

namespace IlpGafferMovie {

size_t MovieReader::FirstPlugIndex = 0U;

MovieReader::MovieReader(const std::string &name) : GafferImage::ImageNode(name)
{
  storeIndexOfNextChild(FirstPlugIndex);
  // addChild(new ImagePlug("in", Plug::In));
  addChild(new Gaffer::StringPlug("fileName"));

  // constexpr auto plug_default = static_cast<unsigned int>(Plug::Default);
  // constexpr auto plug_serializable = static_cast<unsigned int>(Plug::Serialisable);
  // addChild(new ImagePlug("out", Plug::Out, plug_default & ~plug_serializable));

  // outPlug()->setInput(inPlug());
}

void MovieReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  ImageNode::affects(input, outputs);
}

void MovieReader::hash(const Gaffer::ValuePlug *output,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hash(output, context, h);

  // if (output == intermediateColorSpacePlug()) {
  //   intermediateMetadataPlug()->hash(h);
  //   colorSpacePlug()->hash(h);
  //   fileNamePlug()->hash(h);
  //   h.append(OpenColorIOAlgo::currentConfigHash());
  // }
}

#if 0
void MovieReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  try {
    IECore::msg(IECore::Msg::Info, "MovieReader", "compute");
    const auto dwin = outPlug()->dataWindow();
    IECore::msg(IECore::Msg::Info,
      "MovieReader - data window: ",
      boost::format("{%1, %2}, {%3, %4}") % dwin.min.x % dwin.min.y % dwin.max.x % dwin.max.y);


    auto img_ptr = GafferImage::ImageAlgo::image(outPlug(), &ImagePlug::defaultViewName);

    IECore::msg(IECore::Msg::Info,
      "MovieReader - img_ptr: ",
      (img_ptr.get() != nullptr ? std::string{ "yes" } : std::string{ "no" }));

    if (img_ptr == nullptr) { return; }

    auto *r = img_ptr->getChannel<float>("R");
    auto *g = img_ptr->getChannel<float>("G");
    auto *b = img_ptr->getChannel<float>("B");

    IECore::msg(IECore::Msg::Info,
      "MovieReader - r: ",
      (r != nullptr ? std::string{ "yes" } : std::string{ "no" }));
    IECore::msg(IECore::Msg::Info,
      "MovieReader - g: ",
      (g != nullptr ? std::string{ "yes" } : std::string{ "no" }));
    IECore::msg(IECore::Msg::Info,
      "MovieReader - b: ",
      (b != nullptr ? std::string{ "yes" } : std::string{ "no" }));

    // outPlug()->channelData()->writable()
    static const bool no = false;
    if (no) {

    } else {
      ImageNode::compute(output, context);
    }
  } catch (const IECore::Exception &e) {
    IECore::msg(IECore::Msg::Info, "MovieReader - Exception: ", e.what());
  }
}
#endif

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

}// namespace IlpGafferMovie