
#if 0
// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `myproject`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>
#endif

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <ilp_gaffer_movie/movie_reader.hpp>

#include <internal/trace.hpp>

#include <GafferImage/ColorSpace.h>
#include <GafferImage/OpenColorIOAlgo.h>
//#include <GafferImage/OpenImageIOReader.h>

#include <Gaffer/StringPlug.h>

#include <GafferImage/FormatPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImagePlug.h>

#include <OpenColorIO/OpenColorIO.h>

#include <array>
#include <mutex>// TMP!?
#include <string>
#include <vector>

#include <ilp_gaffer_movie/av_reader.hpp>

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
  using BoolPlug = Gaffer::BoolPlug;
  using IntPlug = Gaffer::IntPlug;
  using IntVectorDataPlug = Gaffer::IntVectorDataPlug;
  using StringPlug = Gaffer::StringPlug;
  using ValuePlug = Gaffer::ValuePlug;
  using ValuePlugPtr = Gaffer::ValuePlugPtr;
  using AtomicCompoundDataPlug = Gaffer::AtomicCompoundDataPlug;
  using ImagePlug = GafferImage::ImagePlug;
  using ColorSpace = GafferImage::ColorSpace;
  using ColorSpacePtr = GafferImage::ColorSpacePtr;

  // Please LINTer, it doesn't like bit-wise operations on signed integer types.
  constexpr auto kPlugDefault = static_cast<unsigned int>(Plug::Default);
  constexpr auto kPlugSerialisable = static_cast<unsigned int>(Plug::Serialisable);

  storeIndexOfNextChild(FirstPlugIndex);
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
    /*maxValue=*/static_cast<int>(MissingFrameMode::Hold)
  ));
  ValuePlugPtr startPlug = new ValuePlug(
    /*name=*/"start",
    /*direction=*/Plug::In);
  startPlug->addChild(new IntPlug(// [3][0]
    /*name=*/"mode",
    /*direction=*/Plug::In,
    /*defaultValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*minValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*maxValue=*/static_cast<int>(FrameMaskMode::kClampToFrame)));
  startPlug->addChild(new IntPlug(// [3][1]
    /*name=*/"frame",
    /*direction=*/Plug::In,
    /*defaultValue=*/0));
  addChild(startPlug);// [3]

  ValuePlugPtr endPlug = new ValuePlug(
    /*name=*/"end",
    /*direction=*/Plug::In);
  endPlug->addChild(new IntPlug(// [4][0]
    /*name=*/"mode",
    /*direction=*/Plug::In,
    /*defaultValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*minValue=*/static_cast<int>(FrameMaskMode::kNone),
    /*maxValue=*/static_cast<int>(FrameMaskMode::kClampToFrame)));
  endPlug->addChild(new IntPlug(// [4][1]
    /*name=*/"frame",
    /*direction=*/Plug::In,
    /*defaultValue=*/0));
  addChild(endPlug);// [4]

  addChild(new IntPlug(// [5]
    /*name=*/"videoStreamIndex",
    /*direction=*/Plug::In,
    /*defaultValue=*/-1));
  addChild(new StringPlug(// [6]
    /*name=*/"filterGraph",
    /*direction=*/Plug::In,
    /*defaultValue=*/"vflip"));

  addChild(new StringPlug(// [7]
    /*name=*/"colorSpace",
    /*direction=*/Plug::In));

  addChild(new IntVectorDataPlug(// [8]
    /*name=*/"availableFrames",
    /*direction=*/Plug::Out,
    /*defaultValue=*/new IECore::IntVectorData,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new BoolPlug(// [9]
    /*name=*/"fileValid",
    /*direction=*/Plug::Out,
    /*defaultValue=*/false,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));

  addChild(new BoolPlug(// [10]
    /*name=*/"__intermediateFileValid",
    /*direction=*/Plug::In,
    /*defaultValue=*/false,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new AtomicCompoundDataPlug(// [11]
    /*name=*/"__intermediateMetadata",
    /*direction=*/Plug::In,
    /*defaultValue=*/new IECore::CompoundData,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new StringPlug(// [12]
    /*name=*/"__intermediateColorSpace",
    /*direction=*/Plug::Out,
    /*defaultValue=*/"",
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new ImagePlug(// [13]
    /*name=*/"__intermediateImage",
    /*direction=*/Plug::In,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));

  // We don't really do much work ourselves - we just
  // defer to internal nodes to do the hard work.

  AvReaderPtr avReader = new AvReader(/*name=*/"__avReader");
  addChild(avReader);// [14]
  avReader->fileNamePlug()->setInput(fileNamePlug());
  avReader->refreshCountPlug()->setInput(refreshCountPlug());
  avReader->missingFrameModePlug()->setInput(missingFrameModePlug());
  avReader->videoStreamIndexPlug()->setInput(videoStreamIndexPlug());
  avReader->filterGraphPlug()->setInput(filterGraphPlug());
  _intermediateMetadataPlug()->setInput(avReader->outPlug()->metadataPlug());
  _intermediateFileValidPlug()->setInput(avReader->fileValidPlug());

  ColorSpacePtr colorSpace = new ColorSpace(/*name=*/"__colorSpace");
  addChild(colorSpace);// [15]
  colorSpace->inPlug()->setInput(avReader->outPlug());
  colorSpace->inputSpacePlug()->setInput(_intermediateColorSpacePlug());
  colorSpace->processUnpremultipliedPlug()->setValue(true);
  _intermediateImagePlug()->setInput(colorSpace->outPlug());

  availableFramesPlug()->setInput(avReader->availableFramesPlug());
}

// clang-format off
// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL(name, type, index)          \
  type *MovieReader::name()                          \
  {                                                  \
    return getChild<type>(FirstPlugIndex + (index)); \
  }                                                  \
  const type *MovieReader::name() const              \
  {                                                  \
    return getChild<type>(FirstPlugIndex + (index)); \
  }

// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL_SUB(name, type, index, sub_index)         \
  type *MovieReader::name()                                        \
  {                                                                \
    return getChild<Gaffer::ValuePlug>(FirstPlugIndex + (index))   \
      ->getChild<type>(sub_index);                                 \
  }                                                                \
  const type *MovieReader::name() const                            \
  {                                                                \
    return getChild<Gaffer::ValuePlug>(FirstPlugIndex + (index))   \
      ->getChild<type>(sub_index);                                 \
  }
// clang-format on

PLUG_MEMBER_IMPL(fileNamePlug, Gaffer::StringPlug, 0U);
PLUG_MEMBER_IMPL(refreshCountPlug, Gaffer::IntPlug, 1U);
PLUG_MEMBER_IMPL(missingFrameModePlug, Gaffer::IntPlug, 2U);
PLUG_MEMBER_IMPL_SUB(startModePlug, Gaffer::IntPlug, 3U, 0U);
PLUG_MEMBER_IMPL_SUB(startFramePlug, Gaffer::IntPlug, 3U, 1U);
PLUG_MEMBER_IMPL_SUB(endModePlug, Gaffer::IntPlug, 4U, 0U);
PLUG_MEMBER_IMPL_SUB(endFramePlug, Gaffer::IntPlug, 4U, 1U);

PLUG_MEMBER_IMPL(videoStreamIndexPlug, Gaffer::IntPlug, 5U);
PLUG_MEMBER_IMPL(filterGraphPlug, Gaffer::StringPlug, 6U);

PLUG_MEMBER_IMPL(colorSpacePlug, Gaffer::StringPlug, 7U);
PLUG_MEMBER_IMPL(availableFramesPlug, Gaffer::IntVectorDataPlug, 8U);
PLUG_MEMBER_IMPL(fileValidPlug, Gaffer::BoolPlug, 9U);

PLUG_MEMBER_IMPL(_intermediateFileValidPlug, Gaffer::BoolPlug, 10U);
PLUG_MEMBER_IMPL(_intermediateMetadataPlug, Gaffer::AtomicCompoundDataPlug, 11U);
PLUG_MEMBER_IMPL(_intermediateColorSpacePlug, Gaffer::StringPlug, 12U);
PLUG_MEMBER_IMPL(_intermediateImagePlug, GafferImage::ImagePlug, 13U);

// Not really plugs, but follow the same pattern (they are also children).
PLUG_MEMBER_IMPL(_avReader, AvReader, 14U);
PLUG_MEMBER_IMPL(_colorSpace, GafferImage::ColorSpace, 15U);

#undef PLUG_MEMBER_IMPL
#undef PLUG_MEMBER_IMPL_SUB

size_t MovieReader::supportedExtensions(std::vector<std::string> &extensions)
{
  AvReader::supportedExtensions(extensions);
  return extensions.size();
}

void MovieReader::setDefaultColorSpaceFunction(DefaultColorSpaceFunction f)
{
  _defaultColorSpaceFunction() = f;// NOLINT
}

MovieReader::DefaultColorSpaceFunction MovieReader::getDefaultColorSpaceFunction()
{
  return _defaultColorSpaceFunction();
}

MovieReader::DefaultColorSpaceFunction &MovieReader::_defaultColorSpaceFunction()
{
  // We deliberately make no attempt to free this, because typically a python
  // function is registered here, and we can't free that at exit because python
  // is already shut down by then.
  static DefaultColorSpaceFunction *g_colorSpaceFunction = new DefaultColorSpaceFunction;// NOLINT
  return *g_colorSpaceFunction;
}

void MovieReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  using ValuePlug = Gaffer::ValuePlug;
  using ImagePlug = GafferImage::ImagePlug;


  ImageNode::affects(input, /*out*/ outputs);
  if (input == _intermediateFileValidPlug()) {
    TRACE("MovieReader", "affects - _intermediateFileValidPlug");

    outputs.push_back(fileValidPlug());
  } else if (input == _intermediateMetadataPlug() || input == colorSpacePlug()) {
    TRACE("MovieReader", "affects - _intermediateMetadataPlug | colorSpacePlug");

    outputs.push_back(_intermediateColorSpacePlug());
  } else if (input->parent<ImagePlug>() == _intermediateImagePlug()) {
    outputs.push_back(outPlug()->getChild<ValuePlug>(input->getName()));
  } else if (
    // clang-format off
    input == startFramePlug() || 
    input == startModePlug() || 
    input == endFramePlug() || 
    input == endModePlug()
    // clang-format on
  ) {
    TRACE("MovieReader", "affects - start | end");

    // outputs.push_back(fileValidPlug());
    for (ValuePlug::Iterator it{ outPlug() }; !it.done(); ++it) { outputs.push_back(it->get()); }
  }
}

void MovieReader::hash(const Gaffer::ValuePlug *output,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  namespace OCIOAlgo = GafferImage::OpenColorIOAlgo;

  ImageNode::hash(output, context, /*out*/ h);

  if (output == _intermediateColorSpacePlug()) {
    TRACE("MovieReader", "hash - _intermediateColorSpacePlug");

    _intermediateMetadataPlug()->hash(/*out*/ h);
    colorSpacePlug()->hash(/*out*/ h);
    fileNamePlug()->hash(/*out*/ h);
    videoStreamIndexPlug()->hash(/*out*/ h);
    h.append(OCIOAlgo::currentConfigHash());
  } else if (output == fileValidPlug()) {
    TRACE("MovieReader", "hash - fileValid");

    // const FrameMaskScope scope(context, this, /*clampBlack=*/true);
    _avReader()->fileValidPlug()->hash(/*out*/ h);
  }
}

void MovieReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{

  using ConstCompoundDataPtr = IECore::ConstCompoundDataPtr;
  using StringData = IECore::StringData;
  using StringPlug = Gaffer::StringPlug;
  using BoolPlug = Gaffer::BoolPlug;
  namespace OCIOAlgo = GafferImage::OpenColorIOAlgo;

  if (output == _intermediateColorSpacePlug()) {
    TRACE("MovieReader", "compute - _intermediateColorSpacePlug");

    std::string colorSpace = colorSpacePlug()->getValue();
    if (colorSpace.empty()) {
      ConstCompoundDataPtr metadata = _intermediateMetadataPlug()->getValue();
      if (const auto *const fileFormatData = metadata->member<StringData>("fileFormat")) {
        const auto *const dataTypeData = metadata->member<StringData>("dataType");
        colorSpace = _defaultColorSpaceFunction()(fileNamePlug()->getValue(),
          fileFormatData->readable(),
          dataTypeData != nullptr ? dataTypeData->readable() : "",
          metadata.get(),
          OCIOAlgo::currentConfig());
      }
    }
    static_cast<StringPlug *>(output)->setValue(colorSpace);// NOLINT
  } else if (output == fileValidPlug()) {
    TRACE("MovieReader", "compute - fileValid");

    // const FrameMaskScope scope(context, this, /*clampBlack=*/true);
    static_cast<BoolPlug *>(output)->setValue(_avReader()->fileValidPlug()->getValue());// NOLINT
  } else {
    //TRACE("MovieReader", "compute - ImageNode");

    ImageNode::compute(output, context);
  }
}

void MovieReader::hashViewNames(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  h = _intermediateImagePlug()->viewNamesPlug()->hash();
}

IECore::ConstStringVectorDataPtr MovieReader::computeViewNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeViewNames");

  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->viewNamesPlug()->getValue();
}

void MovieReader::hashFormat(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  h = _intermediateImagePlug()->formatPlug()->hash();
}

GafferImage::Format MovieReader::computeFormat(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeFormat");

  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->formatPlug()->getValue();
}

void MovieReader::hashDataWindow(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  h = _intermediateImagePlug()->dataWindowPlug()->hash();
}

Imath::Box2i MovieReader::computeDataWindow(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeDataWindow");

  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->dataWindowPlug()->getValue();
}

void MovieReader::hashMetadata(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::kBlackOutside
        ? _intermediateImagePlug()->metadataPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->metadataPlug()->hash();
}

IECore::ConstCompoundDataPtr MovieReader::computeMetadata(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeMetadata");

  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::kBlackOutside
           ? _intermediateImagePlug()->metadataPlug()->defaultValue()
           : _intermediateImagePlug()->metadataPlug()->getValue();
}

void MovieReader::hashDeep(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  if (scope.mode() == FrameMaskMode::kBlackOutside) {
    ImageNode::hashDeep(parent, context, h);
  } else {
    h = _intermediateImagePlug()->deepPlug()->hash();
  }
}

bool MovieReader::computeDeep(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeDeep");

  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::kBlackOutside
           ? _intermediateImagePlug()->deepPlug()->defaultValue()
           : _intermediateImagePlug()->deepPlug()->getValue();

  // ??
  // return false;
}

void MovieReader::hashSampleOffsets(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::kBlackOutside
        ? _intermediateImagePlug()->sampleOffsetsPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->sampleOffsetsPlug()->hash();
}

IECore::ConstIntVectorDataPtr MovieReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeSampleOffsets");

  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::kBlackOutside
           ? _intermediateImagePlug()->sampleOffsetsPlug()->defaultValue()
           : _intermediateImagePlug()->sampleOffsetsPlug()->getValue();
}

void MovieReader::hashChannelNames(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::kBlackOutside
        ? _intermediateImagePlug()->channelNamesPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->channelNamesPlug()->hash();
}

IECore::ConstStringVectorDataPtr MovieReader::computeChannelNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeChannelNames");

  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::kBlackOutside
           ? _intermediateImagePlug()->channelNamesPlug()->defaultValue()
           : _intermediateImagePlug()->channelNamesPlug()->getValue();
}

void MovieReader::hashChannelData(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::kBlackOutside
        ? _intermediateImagePlug()->channelDataPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->channelDataPlug()->hash();
}


IECore::ConstFloatVectorDataPtr MovieReader::computeChannelData(const std::string & /*channelName*/,
  const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  TRACE("MovieReader", "computeChannelData");

  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::kBlackOutside
           ? _intermediateImagePlug()->channelDataPlug()->defaultValue()
           : _intermediateImagePlug()->channelDataPlug()->getValue();
}

}// namespace IlpGafferMovie
