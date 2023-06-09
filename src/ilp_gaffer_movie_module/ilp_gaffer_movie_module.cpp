#include <ilp_gaffer_movie/movie_reader.hpp>
#include <ilp_gaffer_movie/movie_writer.hpp>

#include <GafferDispatchBindings/TaskNodeBinding.h>

#include <boost/python.hpp>

using namespace boost::python;

BOOST_PYTHON_MODULE(_IlpGafferMovie) // NOLINT
{
  // NOLINTNEXTLINE
  GafferBindings::DependencyNodeClass<IlpGafferMovie::MovieReader>();

#if 0
  using MovieReaderWrapper = GafferDispatchBindings::TaskNodeWrapper<IlpGafferMovie::MovieReader>;
  // NOLINTNEXTLINE
  GafferDispatchBindings::TaskNodeClass<IlpGafferMovie::MovieReader, MovieReaderWrapper>();
#endif

  using MovieWriterWrapper = GafferDispatchBindings::TaskNodeWrapper<IlpGafferMovie::MovieWriter>;
  // NOLINTNEXTLINE
  GafferDispatchBindings::TaskNodeClass<IlpGafferMovie::MovieWriter, MovieWriterWrapper>();
}
