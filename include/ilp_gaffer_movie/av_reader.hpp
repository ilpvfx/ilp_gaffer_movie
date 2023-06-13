#pragma once

#include <Gaffer/NumericPlug.h>
#include <GafferImage/ImageNode.h>

#include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include <ilp_gaffer_movie/type_id.hpp>

namespace Gaffer {
IE_CORE_FORWARDDECLARE(StringPlug)
}// namespace Gaffer

namespace IlpGafferMovie {

class ILPGAFFERMOVIE_EXPORT AvReader : public GafferImage::ImageNode
{
public:
  explicit AvReader(const std::string &name = defaultName<AvReader>());
  ~AvReader() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovie::AvReader,
    TypeId::kAvReaderTypeId,
    GafferImage::ImageNode);

  enum class MissingFrameMode : int {
    kError = 0,
    kBlack,
    kHold,
  };

  Gaffer::StringPlug *fileNamePlug();
  const Gaffer::StringPlug *fileNamePlug() const;

  // Number of times the node has been refreshed.
  Gaffer::IntPlug *refreshCountPlug();
  const Gaffer::IntPlug *refreshCountPlug() const;

  Gaffer::IntPlug *missingFrameModePlug();
  const Gaffer::IntPlug *missingFrameModePlug() const;

  Gaffer::IntVectorDataPlug *availableFramesPlug();
  const Gaffer::IntVectorDataPlug *availableFramesPlug() const;

#if 0
  Gaffer::IntPlug *channelInterpretationPlug();
  const Gaffer::IntPlug *channelInterpretationPlug() const;
#endif

  void affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const override;

protected:
  void hash(const Gaffer::ValuePlug *output,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;

  void compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const override;
  // Gaffer::ValuePlug::CachePolicy computeCachePolicy( const Gaffer::ValuePlug *output ) const
  // override;

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
  Gaffer::ObjectVectorPlug *_tileBatchPlug();
  const Gaffer::ObjectVectorPlug *_tileBatchPlug() const;

  void _plugSet(Gaffer::Plug *plug);

  static std::size_t FirstPlugIndex;
};

IE_CORE_DECLAREPTR(AvReader)

}// namespace IlpGafferMovie
