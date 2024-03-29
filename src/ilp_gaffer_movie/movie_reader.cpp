
#if 0
// This file will be generated automatically when you run the CMake
// configuration step. It creates a namespace called `myproject`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>
#endif

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include "ilp_gaffer_movie/movie_reader.hpp"

#include <GafferImage/ColorSpace.h>
#include <GafferImage/OpenColorIOAlgo.h>

#include <Gaffer/StringPlug.h>

#include <GafferImage/FormatPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImagePlug.h>

#include <OpenColorIO/OpenColorIO.h>

#include <array>
#include <string>
#include <vector>

#include "ilp_gaffer_movie/av_reader.hpp"
#include "internal/trace.hpp"

namespace {

class FrameMaskScope : public Gaffer::Context::EditableScope
{
public:
  FrameMaskScope(const Gaffer::Context *context,
    const IlpGafferMovie::MovieReader *reader,
    bool clampBlack = false)
    : EditableScope(context), _mode(IlpGafferMovie::MovieReader::FrameMaskMode::None)
  {
    using MovieReader = IlpGafferMovie::MovieReader;

    const int startFrame = reader->startFramePlug()->getValue();
    const int endFrame = reader->endFramePlug()->getValue();
    const auto frame = static_cast<int>(context->getFrame());

    if (frame < startFrame) {
      _mode = static_cast<MovieReader::FrameMaskMode>(reader->startModePlug()->getValue());
    } else if (frame > endFrame) {
      _mode = static_cast<MovieReader::FrameMaskMode>(reader->endModePlug()->getValue());
    }

    if (_mode == MovieReader::FrameMaskMode::BlackOutside && clampBlack) {
      _mode = MovieReader::FrameMaskMode::ClampToFrame;
    }

    if (_mode == MovieReader::FrameMaskMode::ClampToFrame) {
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

size_t MovieReader::g_FirstPlugIndex = 0U;

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

  storeIndexOfNextChild(g_FirstPlugIndex);
  
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

  ValuePlugPtr startPlug = new ValuePlug(
    /*name=*/"start",
    /*direction=*/Plug::In);
  startPlug->addChild(new IntPlug(// [3][0]
    /*name=*/"mode",
    /*direction=*/Plug::In,
    /*defaultValue=*/static_cast<int>(FrameMaskMode::None),
    /*minValue=*/static_cast<int>(FrameMaskMode::None),
    /*maxValue=*/static_cast<int>(FrameMaskMode::ClampToFrame)));
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
    /*defaultValue=*/static_cast<int>(FrameMaskMode::None),
    /*minValue=*/static_cast<int>(FrameMaskMode::None),
    /*maxValue=*/static_cast<int>(FrameMaskMode::ClampToFrame)));
  endPlug->addChild(new IntPlug(// [4][1]
    /*name=*/"frame",
    /*direction=*/Plug::In,
    /*defaultValue=*/0));
  addChild(endPlug);// [4]

  addChild(new StringPlug(// [5]
    /*name=*/"colorSpace",
    /*direction=*/Plug::In));

  addChild(new StringPlug(// [6]
    /*name=*/"videoStream",
    /*direction=*/Plug::In,
    /*defaultValue=*/"best"));
  addChild(new StringPlug(// [7]
    /*name=*/"filterGraph",
    /*direction=*/Plug::In,
    /*defaultValue=*/"vflip"));

  // Please the LINTer, it doesn't like bit-wise operations on signed integer types.
  constexpr auto kPlugDefault = static_cast<unsigned int>(Plug::Default);
  constexpr auto kPlugSerialisable = static_cast<unsigned int>(Plug::Serialisable);

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
  addChild(new StringPlug(// [10]
    /*name=*/"probe",
    /*direction=*/Plug::Out,
    /*defaultValue=*/"",
    /*flags=*/kPlugDefault & ~kPlugSerialisable));

  addChild(new BoolPlug(// [11]
    /*name=*/"__intermediateFileValid",
    /*direction=*/Plug::In,
    /*defaultValue=*/false,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new AtomicCompoundDataPlug(// [12]
    /*name=*/"__intermediateMetadata",
    /*direction=*/Plug::In,
    /*defaultValue=*/new IECore::CompoundData,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new StringPlug(// [13]
    /*name=*/"__intermediateColorSpace",
    /*direction=*/Plug::Out,
    /*defaultValue=*/"",
    /*flags=*/kPlugDefault & ~kPlugSerialisable));
  addChild(new ImagePlug(// [14]
    /*name=*/"__intermediateImage",
    /*direction=*/Plug::In,
    /*flags=*/kPlugDefault & ~kPlugSerialisable));

  // We don't really do much work ourselves - we just
  // defer to internal nodes to do the hard work.

  AvReaderPtr avReader = new AvReader(/*name=*/"__avReader");
  addChild(avReader);// [15]
  ColorSpacePtr colorSpace = new ColorSpace(/*name=*/"__colorSpace");
  addChild(colorSpace);// [16]

  // NOTE(tohi):
  // Add all children before using the member functions to get
  // the plugs so that we know that all indices are valid.

  avReader->fileNamePlug()->setInput(fileNamePlug());
  avReader->refreshCountPlug()->setInput(refreshCountPlug());
  avReader->missingFrameModePlug()->setInput(missingFrameModePlug());
  avReader->videoStreamPlug()->setInput(videoStreamPlug());
  avReader->filterGraphPlug()->setInput(filterGraphPlug());
  _intermediateMetadataPlug()->setInput(avReader->outPlug()->metadataPlug());
  _intermediateFileValidPlug()->setInput(avReader->fileValidPlug());

  colorSpace->inPlug()->setInput(avReader->outPlug());
  colorSpace->inputSpacePlug()->setInput(_intermediateColorSpacePlug());
  colorSpace->processUnpremultipliedPlug()->setValue(true);
  _intermediateImagePlug()->setInput(colorSpace->outPlug());

  availableFramesPlug()->setInput(avReader->availableFramesPlug());
  probePlug()->setInput(avReader->probePlug());
}

// clang-format off
// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL(name, type, index)            \
  type *MovieReader::name()                            \
  {                                                    \
    return getChild<type>(g_FirstPlugIndex + (index)); \
  }                                                    \
  const type *MovieReader::name() const                \
  {                                                    \
    return getChild<type>(g_FirstPlugIndex + (index)); \
  }

// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL_SUB(name, type, index, sub_index)           \
  type *MovieReader::name()                                          \
  {                                                                  \
    return getChild<Gaffer::ValuePlug>(g_FirstPlugIndex + (index))   \
      ->getChild<type>(sub_index);                                   \
  }                                                                  \
  const type *MovieReader::name() const                              \
  {                                                                  \
    return getChild<Gaffer::ValuePlug>(g_FirstPlugIndex + (index))   \
      ->getChild<type>(sub_index);                                   \
  }
// clang-format on

PLUG_MEMBER_IMPL(fileNamePlug, Gaffer::StringPlug, 0U);
PLUG_MEMBER_IMPL(refreshCountPlug, Gaffer::IntPlug, 1U);
PLUG_MEMBER_IMPL(missingFrameModePlug, Gaffer::IntPlug, 2U);
PLUG_MEMBER_IMPL_SUB(startModePlug, Gaffer::IntPlug, 3U, 0U);
PLUG_MEMBER_IMPL_SUB(startFramePlug, Gaffer::IntPlug, 3U, 1U);
PLUG_MEMBER_IMPL_SUB(endModePlug, Gaffer::IntPlug, 4U, 0U);
PLUG_MEMBER_IMPL_SUB(endFramePlug, Gaffer::IntPlug, 4U, 1U);
PLUG_MEMBER_IMPL(colorSpacePlug, Gaffer::StringPlug, 5U);

PLUG_MEMBER_IMPL(videoStreamPlug, Gaffer::StringPlug, 6U);
PLUG_MEMBER_IMPL(filterGraphPlug, Gaffer::StringPlug, 7U);

PLUG_MEMBER_IMPL(availableFramesPlug, Gaffer::IntVectorDataPlug, 8U);
PLUG_MEMBER_IMPL(fileValidPlug, Gaffer::BoolPlug, 9U);
PLUG_MEMBER_IMPL(probePlug, Gaffer::StringPlug, 10U);

PLUG_MEMBER_IMPL(_intermediateFileValidPlug, Gaffer::BoolPlug, 11U);
PLUG_MEMBER_IMPL(_intermediateMetadataPlug, Gaffer::AtomicCompoundDataPlug, 12U);
PLUG_MEMBER_IMPL(_intermediateColorSpacePlug, Gaffer::StringPlug, 13U);
PLUG_MEMBER_IMPL(_intermediateImagePlug, GafferImage::ImagePlug, 14U);

// Not really plugs, but follow the same pattern (they are also children).
PLUG_MEMBER_IMPL(_avReader, AvReader, 15U);
PLUG_MEMBER_IMPL(_colorSpace, GafferImage::ColorSpace, 16U);

#undef PLUG_MEMBER_IMPL
#undef PLUG_MEMBER_IMPL_SUB

size_t MovieReader::supportedExtensions(std::vector<std::string> &extensions)
{
  AvReader::supportedExtensions(extensions);
  return extensions.size();
}

void MovieReader::affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const
{
  using ValuePlug = Gaffer::ValuePlug;
  using ImagePlug = GafferImage::ImagePlug;

  // clang-format off
  ImageNode::affects(input, /*out*/ outputs);
  if (input == _intermediateFileValidPlug()) {
    outputs.push_back(fileValidPlug());
  } else if (input == _intermediateMetadataPlug() || input == colorSpacePlug()) {
    outputs.push_back(_intermediateColorSpacePlug());
  } else if (input->parent<ImagePlug>() == _intermediateImagePlug()) {
    outputs.push_back(outPlug()->getChild<ValuePlug>(input->getName()));
  } else if (input == startFramePlug() || input == startModePlug() || 
             input == endFramePlug()   || input == endModePlug()) {
    outputs.push_back(fileValidPlug());
    for (ValuePlug::Iterator it{ outPlug() }; !it.done(); ++it) { 
      outputs.push_back(it->get()); 
    }
  }
  // clang-format on
}

void MovieReader::hash(const Gaffer::ValuePlug *output,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  ImageNode::hash(output, context, /*out*/ h);

  if (output == _intermediateColorSpacePlug()) {
    _intermediateMetadataPlug()->hash(/*out*/ h);
    colorSpacePlug()->hash(/*out*/ h);
    fileNamePlug()->hash(/*out*/ h);
    videoStreamPlug()->hash(/*out*/ h);
    h.append(GafferImage::OpenColorIOAlgo::currentConfigHash());
  } else if (output == fileValidPlug()) {
    const FrameMaskScope scope(context, this, /*clampBlack=*/true);
    _avReader()->fileValidPlug()->hash(/*out*/ h);
  }
}

void MovieReader::compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const
{
  // using ConstCompoundDataPtr = IECore::ConstCompoundDataPtr;
  // using StringData = IECore::StringData;
  using StringPlug = Gaffer::StringPlug;
  using BoolPlug = Gaffer::BoolPlug;

  if (output == _intermediateColorSpacePlug()) {
    std::string colorSpace = colorSpacePlug()->getValue();
    if (colorSpace.empty()) {
      // The user has selected "Automatic" color space, we need to select one.
      // We could possibly inspect metadata to make a guess, or simply choose a
      // suitable OCIO role, which is the current approach.
      //
      // Metadata extraction:
      // --------------------
      // ConstCompoundDataPtr metadata = _intermediateMetadataPlug()->getValue();
      // if (const auto *const fileFormatData = metadata->member<StringData>("fileFormat")) {
      //   <do something based on metadata>
      // }
      //
      // The idea here is that since we ask for RGB colors in 32-bit float format
      // from the decoder, those pixels should already be converted all the way 
      // to sRGB.

      auto cfg = GafferImage::OpenColorIOAlgo::currentConfig();
      if (cfg != nullptr) {
        auto cs_ptr = cfg->getColorSpace(OCIO_NAMESPACE::ROLE_COLOR_PICKING);
        if (cs_ptr != nullptr) { colorSpace = cs_ptr->getName(); }
      }
    }
    static_cast<StringPlug *>(output)->setValue(colorSpace);// NOLINT
  } else if (output == fileValidPlug()) {
    const FrameMaskScope scope(context, this, /*clampBlack=*/true);
    static_cast<BoolPlug *>(output)->setValue(_avReader()->fileValidPlug()->getValue());// NOLINT
  } else {
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
  const FrameMaskScope scope{ context, this, /*clampBlack=*/true };
  return _intermediateImagePlug()->dataWindowPlug()->getValue();
}

void MovieReader::hashMetadata(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::BlackOutside
        ? _intermediateImagePlug()->metadataPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->metadataPlug()->hash();
}

IECore::ConstCompoundDataPtr MovieReader::computeMetadata(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::BlackOutside
           ? _intermediateImagePlug()->metadataPlug()->defaultValue()
           : _intermediateImagePlug()->metadataPlug()->getValue();
}

void MovieReader::hashDeep(const GafferImage::ImagePlug *parent,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  // NOTE(tohi):
  // See comment in computeDeep(...) function below, it's possible
  // that we should "do nothing" here, and claim that "deep" is not
  // supported for video frames.

  const FrameMaskScope scope{ context, this };
  if (scope.mode() == FrameMaskMode::BlackOutside) {
    ImageNode::hashDeep(parent, context, h);
  } else {
    h = _intermediateImagePlug()->deepPlug()->hash();
  }
}

bool MovieReader::computeDeep(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  // NOTE(tohi):
  // Should we just return 'false' and claim that "deep" is not
  // appilcable to video frames?

  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::BlackOutside
           ? _intermediateImagePlug()->deepPlug()->defaultValue()
           : _intermediateImagePlug()->deepPlug()->getValue();
}

void MovieReader::hashSampleOffsets(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::BlackOutside
        ? _intermediateImagePlug()->sampleOffsetsPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->sampleOffsetsPlug()->hash();
}

IECore::ConstIntVectorDataPtr MovieReader::computeSampleOffsets(const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::BlackOutside
           ? _intermediateImagePlug()->sampleOffsetsPlug()->defaultValue()
           : _intermediateImagePlug()->sampleOffsetsPlug()->getValue();
}

void MovieReader::hashChannelNames(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::BlackOutside
        ? _intermediateImagePlug()->channelNamesPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->channelNamesPlug()->hash();
}

IECore::ConstStringVectorDataPtr MovieReader::computeChannelNames(const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::BlackOutside
           ? _intermediateImagePlug()->channelNamesPlug()->defaultValue()
           : _intermediateImagePlug()->channelNamesPlug()->getValue();
}

void MovieReader::hashChannelData(const GafferImage::ImagePlug * /*parent*/,
  const Gaffer::Context *context,
  IECore::MurmurHash &h) const
{
  const FrameMaskScope scope{ context, this };
  h = scope.mode() == FrameMaskMode::BlackOutside
        ? _intermediateImagePlug()->channelDataPlug()->defaultValue()->Object::hash()
        : _intermediateImagePlug()->channelDataPlug()->hash();
}

IECore::ConstFloatVectorDataPtr MovieReader::computeChannelData(const std::string & /*channelName*/,
  const Imath::V2i & /*tileOrigin*/,
  const Gaffer::Context *context,
  const GafferImage::ImagePlug * /*parent*/) const
{
  const FrameMaskScope scope{ context, this };
  return scope.mode() == FrameMaskMode::BlackOutside
           ? _intermediateImagePlug()->channelDataPlug()->defaultValue()
           : _intermediateImagePlug()->channelDataPlug()->getValue();
}

}// namespace IlpGafferMovie
