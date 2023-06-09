#pragma once

#include <string>

#include <GafferImage/ImageNode.h>

#include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include <ilp_gaffer_movie/type_id.hpp>

// ENV
// export IECORE_LOG_LEVEL=Info
// export GAFFER_EXTENSION_PATHS=~/dev/git/gaffer_extension_example/install:$GAFFER_EXTENSION_PATHS

namespace Gaffer {
IE_CORE_FORWARDDECLARE(StringPlug)
}// namespace Gaffer

#if 0
namespace GafferImage {
IE_CORE_FORWARDDECLARE(ImagePlug)
}// namespace GafferImage
#endif

namespace IlpGafferMovie {

class ILPGAFFERMOVIE_EXPORT MovieReader : public GafferImage::ImageNode
{
public:
  explicit MovieReader(const std::string &name = defaultName<MovieReader>());
  ~MovieReader() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovie::MovieReader,
    TypeId::kMovieReaderTypeId,
    GafferImage::ImageNode)

  Gaffer::StringPlug *fileNamePlug();
  const Gaffer::StringPlug *fileNamePlug() const;

  void affects(const Gaffer::Plug *input, AffectedPlugsContainer &outputs) const override;

protected:
  void hash(const Gaffer::ValuePlug *output,
    const Gaffer::Context *context,
    IECore::MurmurHash &h) const override;

  // void compute(Gaffer::ValuePlug *output, const Gaffer::Context *context) const override;

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

  // GafferImage::ImagePlug *inPlug();
  // const GafferImage::ImagePlug *inPlug() const;
  //
  // GafferImage::ImagePlug *outPlug();
  // const GafferImage::ImagePlug *outPlug() const;

private:
  static std::size_t FirstPlugIndex;
};

IE_CORE_DECLAREPTR(MovieReader)

}// namespace IlpGafferMovie
