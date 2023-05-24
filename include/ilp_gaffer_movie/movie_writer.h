#pragma once

#include <string>

#include <Gaffer/NumericPlug.h>
#include <Gaffer/StringPlug.h>
#include <GafferDispatch/TaskNode.h>
#include <GafferImage/ImagePlug.h>

#include "ilp_gaffer_movie_writer/type_id.h"

// ENV
// export IECORE_LOG_LEVEL=Info
// export GAFFER_EXTENSION_PATHS=~/dev/git/gaffer_extension_example/install:$GAFFER_EXTENSION_PATHS

#if 0
namespace Gaffer {
//IE_CORE_FORWARDDECLARE(IntPlug)
IE_CORE_FORWARDDECLARE(StringPlug)
IE_CORE_FORWARDDECLARE(ValuePlug)
} // namespace Gaffer
#endif

namespace GafferImage {
// IE_CORE_FORWARDDECLARE(ColorSpace)
IE_CORE_FORWARDDECLARE(ImagePlug)
} // namespace GafferImage

namespace IlpGafferMovieWriter {

class MovieWriterSequential : public GafferDispatch::TaskNode {
public:
  MovieWriterSequential(const std::string &name = defaultName<MovieWriterSequential>());
  ~MovieWriterSequential() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovieWriter::MovieWriterSequential,
                           TypeId::kIlpGafferMovieWriterSequentialTypeId,
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

  // Re-implemented to return true, since the entire file must be written at once.
  bool requiresSequenceExecution() const override;

private:
  static std::size_t FirstPlugIndex;

  // Friendship for the bindings.
  friend struct GafferDispatchBindings::Detail::TaskNodeAccessor;
};

IE_CORE_DECLAREPTR(MovieWriterSequential)

} // namespace IlpGafferMovieWriter

