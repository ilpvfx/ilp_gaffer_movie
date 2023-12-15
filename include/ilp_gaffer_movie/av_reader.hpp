#pragma once

#include <GafferImage/ImageNode.h>

#include <Gaffer/NumericPlug.h>

#include <memory>
#include <string>// std::string
#include <vector>// std::vector


#include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include <ilp_gaffer_movie/type_id.hpp>

namespace Gaffer {
IE_CORE_FORWARDDECLARE(StringPlug)
}// namespace Gaffer

namespace IlpGafferMovie {

// Convenience macro for declaring plug member functions.
#define PLUG_MEMBER_DECL(name, type) \
  type *name();                      \
  const type *name() const;

class ILPGAFFERMOVIE_EXPORT AvReader : public GafferImage::ImageNode
{
public:
  explicit AvReader(const std::string &name = defaultName<AvReader>());
  ~AvReader() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovie::AvReader,
    TypeId::kAvReaderTypeId,
    GafferImage::ImageNode);

  enum MissingFrameMode {
    Error = 0,
    Black,
    Hold,
  };

  PLUG_MEMBER_DECL(fileNamePlug, Gaffer::StringPlug);

  // Number of times the node has been refreshed.
  PLUG_MEMBER_DECL(refreshCountPlug, Gaffer::IntPlug);

  PLUG_MEMBER_DECL(videoStreamIndexPlug, Gaffer::IntPlug);
  PLUG_MEMBER_DECL(filterGraphPlug, Gaffer::StringPlug);

  PLUG_MEMBER_DECL(missingFrameModePlug, Gaffer::IntPlug);

  PLUG_MEMBER_DECL(availableVideoStreamInfoPlug, Gaffer::StringVectorDataPlug);
  PLUG_MEMBER_DECL(availableVideoStreamIndicesPlug, Gaffer::IntVectorDataPlug);

  PLUG_MEMBER_DECL(availableFramesPlug, Gaffer::IntVectorDataPlug);
  PLUG_MEMBER_DECL(fileValidPlug, Gaffer::BoolPlug);

  void affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const override;

  static size_t supportedExtensions(std::vector<std::string> &extensions);

protected:
  void hash(const Gaffer::ValuePlug *output,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;

  void compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const override;
  Gaffer::ValuePlug::CachePolicy computeCachePolicy(const Gaffer::ValuePlug *output) const override;

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
  std::shared_ptr<void> _retrieveDecoder(const Gaffer::Context *context) const;

  std::shared_ptr<void> _retrieveFrame(
    const Gaffer::Context *context /*, bool holdForBlack = false*/) const;

  //PLUG_MEMBER_DECL(_tileBatchPlug, Gaffer::ObjectVectorPlug);

  void _hashFileName(const Gaffer::Context *context, IECore::MurmurHash &h) const;

  void _plugSet(Gaffer::Plug *plug);

  static size_t g_firstPlugIndex;
};

#undef PLUG_MEMBER_DECL

IE_CORE_DECLAREPTR(AvReader)

}// namespace IlpGafferMovie
