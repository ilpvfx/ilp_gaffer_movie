#pragma once

#include <string>

#include <Gaffer/NumericPlug.h>
#include <Gaffer/StringPlug.h>
#include <GafferDispatch/TaskNode.h>
#include <GafferImage/ImagePlug.h>

// #include <ilp_gaffer_movie/ilp_gaffer_movie_export.hpp>
#include "ilp_gaffer_movie/ilp_gaffer_movie_export.hpp"
#include "ilp_gaffer_movie/type_id.hpp"

namespace IlpGafferMovie {

// Convenience macro for declaring plug member functions.
#define PLUG_MEMBER_DECL(name, type) \
  type *name();                      \
  const type *name() const;

class ILPGAFFERMOVIE_EXPORT MovieWriter : public GafferDispatch::TaskNode
{
public:
  explicit MovieWriter(const std::string &name = defaultName<MovieWriter>());
  ~MovieWriter() override = default;

  GAFFER_NODE_DECLARE_TYPE(IlpGafferMovie::MovieWriter,
    TypeId::kMovieWriterTypeId,
    GafferDispatch::TaskNode)

  IECore::MurmurHash hash(const Gaffer::Context *context) const override;

  PLUG_MEMBER_DECL(inPlug, GafferImage::ImagePlug);
  PLUG_MEMBER_DECL(fileNamePlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(colorRangePlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(colorspacePlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(colorTrcPlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(colorPrimariesPlug, Gaffer::StringPlug);
  PLUG_MEMBER_DECL(filtergraphPlug, Gaffer::StringPlug);

  PLUG_MEMBER_DECL(codecPlug, Gaffer::StringPlug);
  Gaffer::ValuePlug *codecSettingsPlug(const std::string &codecName);
  const Gaffer::ValuePlug *codecSettingsPlug(const std::string &codecName) const;

  PLUG_MEMBER_DECL(outPlug, GafferImage::ImagePlug);

protected:
  void execute() const override;

  void executeSequence(const std::vector<float> &frames) const override;

  // Re-implemented to return true, since the entire movie file must be written at once.
  bool requiresSequenceExecution() const override { return true; } 

private:
  static std::size_t g_FirstPlugIndex;

  // Friendship for the bindings.
  friend struct GafferDispatchBindings::Detail::TaskNodeAccessor;
};

// Keep macro local.
#undef PLUG_MEMBER_DECL

IE_CORE_DECLAREPTR(MovieWriter)

}// namespace IlpGafferMovie
