
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

namespace {

class FrameMaskScope : public Gaffer::Context::EditableScope
{
public:
  FrameMaskScope(const Gaffer::Context *context,
    const IlpGafferMovie::MovieReader *reader,
    bool clampBlack = false)
    : EditableScope(context), _mode(IlpGafferMovie::MovieReader::FrameMaskMode::kNone)
  {
    const int startFrame = reader->startFramePlug()->getValue();
    const int endFrame = reader->endFramePlug()->getValue();
    const auto frame = static_cast<int>(context->getFrame());

    // clang-format off
    if (frame < startFrame) {
      _mode = static_cast<IlpGafferMovie::MovieReader::FrameMaskMode>(
        reader->startModePlug()->getValue());
    } else if (frame > endFrame) {
      _mode = static_cast<IlpGafferMovie::MovieReader::FrameMaskMode>(
        reader->endModePlug()->getValue());
    }
    // clang-format on

    if (_mode == IlpGafferMovie::MovieReader::FrameMaskMode::kBlackOutside && clampBlack) {
      _mode = IlpGafferMovie::MovieReader::FrameMaskMode::kClampToFrame;
    }

    if (_mode == IlpGafferMovie::MovieReader::FrameMaskMode::kClampToFrame) {
      setFrame(static_cast<float>(Imath::clamp(frame, startFrame, endFrame)));
    }
  }

  [[nodiscard]] auto mode() const -> IlpGafferMovie::MovieReader::FrameMaskMode { return _mode; }

private:
  IlpGafferMovie::MovieReader::FrameMaskMode _mode;
};

}// namespace


GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::MovieReader);

namespace IlpGafferMovie {

size_t MovieReader::FirstPlugIndex = 0U;

MovieReader::MovieReader(const std::string &name) : GafferImage::ImageNode(name)
{
  using Plug = Gaffer::Plug;

  constexpr auto kPlugDefault = static_cast<unsigned int>(Plug::Default);
  constexpr auto kPlugSerializable = static_cast<unsigned int>(Plug::Serialisable);

  storeIndexOfNextChild(FirstPlugIndex);
  addChild(new Gaffer::StringPlug("fileName", Plug::In, "", /*flags=*/Plug::Default));
  addChild(new Gaffer::IntPlug("refreshCount"));

  Gaffer::ValuePlugPtr startPlug = new Gaffer::ValuePlug("start", Plug::In);
  startPlug->addChild(new Gaffer::IntPlug("mode",
    Plug::In,
    /*defaultValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*minValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*maxValue=*/static_cast<int>(FrameMaskMode::kClampToFrame)));
  startPlug->addChild(new Gaffer::IntPlug("frame", Plug::In, /*defaultValue=*/0));
  addChild(startPlug);

  Gaffer::ValuePlugPtr endPlug = new Gaffer::ValuePlug("end", Plug::In);
  endPlug->addChild(new Gaffer::IntPlug("mode",
    Plug::In,
    /*defaultValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*minValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*maxValue=*/static_cast<int>(FrameMaskMode::kClampToFrame)));
  endPlug->addChild(new Gaffer::IntPlug("frame", Plug::In, /*defaultValue=*/0));
  addChild(endPlug);

  // clang-format off
  addChild(new Gaffer::AtomicCompoundDataPlug("__intermediateMetadata",
    Plug::In,
    new IECore::CompoundData,
    kPlugDefault & ~kPlugSerializable));
  addChild(new GafferImage::ImagePlug("__intermediateImage", 
    Plug::In, 
    kPlugDefault & ~kPlugSerializable));
  // clang-format on

  // We don't really do much work ourselves - we just
  // defer to internal nodes to do the hard work.

  AvReaderPtr avReader = new AvReader("__avReader");
  addChild(avReader);
  avReader->fileNamePlug()->setInput(fileNamePlug());
  avReader->refreshCountPlug()->setInput(refreshCountPlug());
  _intermediateMetadataPlug()->setInput(avReader->outPlug()->metadataPlug());
  _intermediateImagePlug()->setInput(avReader->outPlug());
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

Gaffer::IntPlug *MovieReader::startModePlug()
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(0);
}

const Gaffer::IntPlug *MovieReader::startModePlug() const
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(0);
}

Gaffer::IntPlug *MovieReader::startFramePlug()
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(1);
}

const Gaffer::IntPlug *MovieReader::startFramePlug() const
{
  constexpr size_t kPlugIndex = 2;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(1);
}

Gaffer::IntPlug *MovieReader::endModePlug()
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(0);
}

const Gaffer::IntPlug *MovieReader::endModePlug() const
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(0);
}

Gaffer::IntPlug *MovieReader::endFramePlug()
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(1);
}

const Gaffer::IntPlug *MovieReader::endFramePlug() const
{
  constexpr size_t kPlugIndex = 3;
  return getChild<Gaffer::ValuePlug>(FirstPlugIndex + kPlugIndex)->getChild<Gaffer::IntPlug>(1);
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
  //IECore::msg(IECore::Msg::Info, "MovieReader", "affects");

  ImageNode::affects(input, outputs);

  /*if (input == _intermediateMetadataPlug() || input == colorSpacePlug()) {
    outputs.push_back(intermediateColorSpacePlug());
  } else*/
  // clang-format off
  if (input->parent<GafferImage::ImagePlug>() == _intermediateImagePlug()) {
    outputs.push_back(outPlug()->getChild<Gaffer::ValuePlug>(input->getName()));
  } else if (input == startFramePlug() || input == startModePlug() || 
             input == endFramePlug() || input == endModePlug()) {
    for (Gaffer::ValuePlug::Iterator it(outPlug()); !it.done(); ++it) { outputs.push_back(it->get()); }
  }
  // clang-format on
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
  IECore::msg(IECore::Msg::Info, "MovieReader", "compute");

  ImageNode::compute(output, context);
}

void MovieReader::hashViewNames(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this, /*clampBlack=*/true };
  h = _intermediateImagePlug()->viewNamesPlug()->hash();
}

IECore::ConstStringVectorDataPtr MovieReader::computeViewNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeViewNames");

  const auto scope = FrameMaskScope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->viewNamesPlug()->getValue();
}

void MovieReader::hashFormat(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this, /*clampBlack=*/true };
  h = _intermediateImagePlug()->formatPlug()->hash();
}

GafferImage::Format MovieReader::computeFormat(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeFormat");

  const auto scope = FrameMaskScope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->formatPlug()->getValue();
}

void MovieReader::hashDataWindow(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this, /*clampBlack=*/true };
  h = _intermediateImagePlug()->dataWindowPlug()->hash();
}

Imath::Box2i MovieReader::computeDataWindow(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeDataWindow");

  const auto scope = FrameMaskScope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->dataWindowPlug()->getValue();
}

void MovieReader::hashMetadata(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    h = _intermediateImagePlug()->metadataPlug()->defaultValue()->Object::hash();
  } else {
    h = _intermediateImagePlug()->metadataPlug()->hash();
  }
}

IECore::ConstCompoundDataPtr MovieReader::computeMetadata(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeMetadata");

  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    return _intermediateImagePlug()->metadataPlug()->defaultValue();
  }
  return _intermediateImagePlug()->metadataPlug()->getValue();
}

void MovieReader::hashDeep(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    ImageNode::hashDeep(parent, context, h);
  } else {
    h = _intermediateImagePlug()->deepPlug()->hash();
  }
}

bool MovieReader::computeDeep(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeDeep");

  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    return _intermediateImagePlug()->deepPlug()->defaultValue();
  }
  return _intermediateImagePlug()->deepPlug()->getValue();

  // ??
  // return false;
}

void MovieReader::hashSampleOffsets(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    h = _intermediateImagePlug()->sampleOffsetsPlug()->defaultValue()->Object::hash();
  } else {
    h = _intermediateImagePlug()->sampleOffsetsPlug()->hash();
  }
}

IECore::ConstIntVectorDataPtr MovieReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeSampleOffsets");

  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    return _intermediateImagePlug()->sampleOffsetsPlug()->defaultValue();
  }
  return _intermediateImagePlug()->sampleOffsetsPlug()->getValue();
}

void MovieReader::hashChannelNames(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    h = _intermediateImagePlug()->channelNamesPlug()->defaultValue()->Object::hash();
  } else {
    h = _intermediateImagePlug()->channelNamesPlug()->hash();
  }
}

IECore::ConstStringVectorDataPtr MovieReader::computeChannelNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeChannelNames");

  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    return _intermediateImagePlug()->channelNamesPlug()->defaultValue();
  }
  return _intermediateImagePlug()->channelNamesPlug()->getValue();
}

void MovieReader::hashChannelData(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    h = _intermediateImagePlug()->channelDataPlug()->defaultValue()->Object::hash();
  } else {
    h = _intermediateImagePlug()->channelDataPlug()->hash();
  }
}

IECore::ConstFloatVectorDataPtr MovieReader::computeChannelData(const std::string & /*channelName*/,
  const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  IECore::msg(IECore::Msg::Info, "MovieReader", "computeChannelData");

  const auto scope = FrameMaskScope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    return _intermediateImagePlug()->channelDataPlug()->defaultValue();
  }
  return _intermediateImagePlug()->channelDataPlug()->getValue();
}

}// namespace IlpGafferMovie