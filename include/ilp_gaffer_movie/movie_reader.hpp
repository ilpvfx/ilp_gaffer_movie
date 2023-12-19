#pragma once

#include <GafferImage/ImageNode.h>

#include <Gaffer/CompoundNumericPlug.h>

#include "OpenColorIO/OpenColorTypes.h"

#include <functional>
#include <string>

#include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include <ilp_gaffer_movie/type_id.hpp>

namespace Gaffer {
IE_CORE_FORWARDDECLARE(StringPlug)
}// namespace Gaffer

namespace GafferImage {
IE_CORE_FORWARDDECLARE(ColorSpace)
}// namespace GafferImage

namespace IlpGafferMovie {

IE_CORE_FORWARDDECLARE(AvReader)

// Convenience macro for declaring plug member functions.
#define PLUG_MEMBER_DECL(name, type) \
  type *name();                      \
  const type *name() const;

class ILPGAFFERMOVIE_EXPORT MovieReader : public GafferImage::ImageNode
{
public:
  explicit MovieReader(const std::string &name = defaultName<MovieReader>());
  ~MovieReader() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovie::MovieReader,
    TypeId::kMovieReaderTypeId,
    GafferImage::ImageNode)

  // The MissingFrameMode controls how to handle missing images.
  // It is distinct from AvReader::MissingFrameMode so
  // that we can provide alternate modes using higher
  // level approaches in the future (e.g interpolation).
  enum MissingFrameMode {
    Error = 0,
    Black,
    Hold,
  };

  // The FrameMaskMode controls how to handle images
  // outside of the values provided by the start
  // and end frame masks.
  enum class FrameMaskMode : int {
    // clang-format off
    kNone         = 0,
    kBlackOutside = 1,
    kClampToFrame = 2,
    // clang-format on
  };

  PLUG_MEMBER_DECL(fileNamePlug, Gaffer::StringPlug);

  // Number of times the node has been refreshed.
  PLUG_MEMBER_DECL(refreshCountPlug, Gaffer::IntPlug);

  PLUG_MEMBER_DECL(missingFrameModePlug, Gaffer::IntPlug);

  PLUG_MEMBER_DECL(videoStreamPlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(filterGraphPlug, Gaffer::StringPlug);

  PLUG_MEMBER_DECL(startModePlug, Gaffer::IntPlug);
  PLUG_MEMBER_DECL(startFramePlug, Gaffer::IntPlug);

  PLUG_MEMBER_DECL(endModePlug, Gaffer::IntPlug);
  PLUG_MEMBER_DECL(endFramePlug, Gaffer::IntPlug);

  PLUG_MEMBER_DECL(colorSpacePlug, Gaffer::StringPlug);

  PLUG_MEMBER_DECL(availableFramesPlug, Gaffer::IntVectorDataPlug);

  PLUG_MEMBER_DECL(fileValidPlug, Gaffer::BoolPlug);

  void affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const override;

  static size_t supportedExtensions(std::vector<std::string> &extensions);

  // A function which can take information about a file being read, and return the colorspace
  // of the data within the file. This is used whenever the colorSpace plug is at its default
  // value.
  using DefaultColorSpaceFunction = std::function<const std::string(const std::string &fileName,
    const std::string &fileFormat,
    const std::string &dataType,
    const IECore::CompoundData *metadata,
    const OCIO_NAMESPACE::ConstConfigRcPtr &config)>;
  static void setDefaultColorSpaceFunction(DefaultColorSpaceFunction f);
  static DefaultColorSpaceFunction getDefaultColorSpaceFunction();

protected:
  void hash(const Gaffer::ValuePlug *output,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;

  void compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const override;

  void hashViewNames(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  IECore::ConstStringVectorDataPtr computeViewNames(const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashFormat(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  GafferImage::Format computeFormat(const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashDataWindow(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  Imath::Box2i computeDataWindow(const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashMetadata(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  IECore::ConstCompoundDataPtr computeMetadata(const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashDeep(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  bool computeDeep(const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashSampleOffsets(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  IECore::ConstIntVectorDataPtr computeSampleOffsets(const Imath::V2i &tileOrigin,
    const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashChannelNames(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  IECore::ConstStringVectorDataPtr computeChannelNames(const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

  void hashChannelData(const GafferImage::ImagePlug *parent,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;
  IECore::ConstFloatVectorDataPtr computeChannelData(const std::string &channelName,
    const Imath::V2i &tileOrigin,
    const Gaffer::Context *context,
    const GafferImage::ImagePlug *parent) const override;

private:
  // We use internal nodes to do all the hard work,
  // but we need to store intermediate results between
  // those nodes in order to affect the outcome.

  PLUG_MEMBER_DECL(_intermediateMetadataPlug, Gaffer::AtomicCompoundDataPlug);
  PLUG_MEMBER_DECL(_intermediateColorSpacePlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(_intermediateImagePlug, GafferImage::ImagePlug);
  PLUG_MEMBER_DECL(_intermediateFileValidPlug, Gaffer::BoolPlug);

  // Not really plugs, but follow the same pattern.
  PLUG_MEMBER_DECL(_avReader, AvReader);
  PLUG_MEMBER_DECL(_colorSpace, GafferImage::ColorSpace);

  static DefaultColorSpaceFunction &_defaultColorSpaceFunction();

  static size_t FirstPlugIndex;
};

#undef PLUG_MEMBER_DECL

IE_CORE_DECLAREPTR(MovieReader)

}// namespace IlpGafferMovie
