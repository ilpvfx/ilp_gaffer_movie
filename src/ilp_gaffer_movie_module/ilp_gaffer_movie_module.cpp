#include <ilp_gaffer_movie/movie_reader.hpp>
#include <ilp_gaffer_movie/movie_writer.hpp>

#include <GafferDispatchBindings/TaskNodeBinding.h>

#include <boost/python.hpp>

using namespace boost::python;

BOOST_PYTHON_MODULE(_IlpGafferMovie)// NOLINT
{
  {
    // NOLINTNEXTLINE
    scope s = GafferBindings::DependencyNodeClass<IlpGafferMovie::MovieReader>();

    // clang-format off
    enum_<IlpGafferMovie::MovieReader::FrameMaskMode>("FrameMaskMode")
      .value("None", IlpGafferMovie::MovieReader::FrameMaskMode::kNone)
      .value("None_", IlpGafferMovie::MovieReader::FrameMaskMode::kNone)
      .value("BlackOutside", IlpGafferMovie::MovieReader::FrameMaskMode::kBlackOutside)
      .value("ClampToFrame", IlpGafferMovie::MovieReader::FrameMaskMode::kClampToFrame)
    ;
    // clang-format on
  }

  {
    using MovieWriterWrapper = GafferDispatchBindings::TaskNodeWrapper<IlpGafferMovie::MovieWriter>;

    // NOLINTNEXTLINE
    scope s =
      GafferDispatchBindings::TaskNodeClass<IlpGafferMovie::MovieWriter, MovieWriterWrapper>();
  }

#if 0
  using MovieReaderWrapper = GafferDispatchBindings::TaskNodeWrapper<IlpGafferMovie::MovieReader>;
  // NOLINTNEXTLINE
  GafferDispatchBindings::TaskNodeClass<IlpGafferMovie::MovieReader, MovieReaderWrapper>();

  using MovieWriterWrapper = GafferDispatchBindings::TaskNodeWrapper<IlpGafferMovie::MovieWriter>;
  // NOLINTNEXTLINE
  GafferDispatchBindings::TaskNodeClass<IlpGafferMovie::MovieWriter, MovieWriterWrapper>();
#endif
}
