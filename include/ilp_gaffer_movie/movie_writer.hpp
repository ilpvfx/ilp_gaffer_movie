#pragma once

#include <string>

#include <Gaffer/NumericPlug.h>
#include <Gaffer/StringPlug.h>
#include <GafferDispatch/TaskNode.h>
#include <GafferImage/ImagePlug.h>

// #include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include <ilp_gaffer_movie/type_id.hpp>

namespace Gaffer {
// IE_CORE_FORWARDDECLARE(IntPlug)
IE_CORE_FORWARDDECLARE(StringPlug)
// IE_CORE_FORWARDDECLARE(ValuePlug)
}// namespace Gaffer

namespace GafferImage {
// IE_CORE_FORWARDDECLARE(ColorSpace)
IE_CORE_FORWARDDECLARE(ImagePlug)
}// namespace GafferImage

namespace IlpGafferMovie {

class ILPGAFFERMOVIE_EXPORT MovieWriter : public GafferDispatch::TaskNode
{
public:
  explicit MovieWriter(const std::string &name = defaultName<MovieWriter>());
  ~MovieWriter() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovie::MovieWriter,
    TypeId::kMovieWriterTypeId,
    GafferDispatch::TaskNode)

  IECore::MurmurHash hash(const Gaffer::Context *context) const override;

  GafferImage::ImagePlug *inPlug();
  const GafferImage::ImagePlug *inPlug() const;

  Gaffer::StringPlug *fileNamePlug();
  const Gaffer::StringPlug *fileNamePlug() const;

  // Gaffer::StringPlug *formatPlug();
  // const Gaffer::StringPlug *formatPlug() const;

  Gaffer::StringPlug *colorRangePlug();
  const Gaffer::StringPlug *colorRangePlug() const;

  Gaffer::StringPlug *colorspacePlug();
  const Gaffer::StringPlug *colorspacePlug() const;

  Gaffer::StringPlug *colorPrimariesPlug();
  const Gaffer::StringPlug *colorPrimariesPlug() const;

  Gaffer::StringPlug *colorTrcPlug();
  const Gaffer::StringPlug *colorTrcPlug() const;

  Gaffer::StringPlug *swsFlagsPlug();
  const Gaffer::StringPlug *swsFlagsPlug() const;

  Gaffer::StringPlug *filtergraphPlug();
  const Gaffer::StringPlug *filtergraphPlug() const;

  Gaffer::StringPlug *codecPlug();
  const Gaffer::StringPlug *codecPlug() const;

  GafferImage::ImagePlug *outPlug();
  const GafferImage::ImagePlug *outPlug() const;

  Gaffer::ValuePlug *codecSettingsPlug(const std::string &codec_name);
  const Gaffer::ValuePlug *codecSettingsPlug(const std::string &codec_name) const;

protected:
  void execute() const override;

  void executeSequence(const std::vector<float> &frames) const override;

  // Re-implemented to return true, since the entire movie file must be written at once.
  bool requiresSequenceExecution() const override;

private:
  static std::size_t FirstPlugIndex;

  // Friendship for the bindings.
  friend struct GafferDispatchBindings::Detail::TaskNodeAccessor;
};

IE_CORE_DECLAREPTR(MovieWriter)

}// namespace IlpGafferMovie
