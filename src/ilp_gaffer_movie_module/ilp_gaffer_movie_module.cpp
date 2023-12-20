#include <ilp_gaffer_movie/av_reader.hpp>
#include <ilp_gaffer_movie/movie_reader.hpp>
#include <ilp_gaffer_movie/movie_writer.hpp>
#include <ilp_gaffer_movie/startup.hpp>

#include <GafferDispatchBindings/TaskNodeBinding.h>

#include "OpenColorIO/OpenColorIO.h"

#include <boost/mpl/vector.hpp>
#include <boost/python.hpp>

using namespace boost::python;

namespace {
struct DefaultColorSpaceFunction
{
  explicit DefaultColorSpaceFunction(object fn) : m_fn(fn) {}// NOLINT

  std::string operator()(const std::string &fileName,
    const std::string &fileFormat,
    const std::string &dataType,
    const IECore::CompoundData *metadata,
    const OCIO_NAMESPACE::ConstConfigRcPtr &config)
  {

    IECorePython::ScopedGILLock gilock;
    try {
      return extract<std::string>(m_fn(fileName,
        fileFormat,
        dataType,
        IECore::CompoundDataPtr(const_cast<IECore::CompoundData *>(metadata)),// NOLINT
        config));
    } catch (const error_already_set &) {
      IECorePython::ExceptionAlgo::translatePythonException();
    }
  }

private:
  object m_fn;
};

template<typename T> void setDefaultColorSpaceFunction(object f)
{
  T::setDefaultColorSpaceFunction(DefaultColorSpaceFunction(f));// NOLINT
}

template<typename T> object getDefaultColorSpaceFunction()
{
  return make_function(T::getDefaultColorSpaceFunction(),
    default_call_policies(),
    boost::mpl::vector<std::string,
      const std::string &,
      const std::string &,
      const std::string &,
      const IECore::CompoundData *,
      const OCIO_NAMESPACE::ConstConfigRcPtr &>());
}

template<typename T> boost::python::list supportedExtensions()
{
  std::vector<std::string> e;
  T::supportedExtensions(e);

  boost::python::list result;
  for (auto &&s : e) { result.append(s); }

  return result;
}

}// namespace

BOOST_PYTHON_MODULE(_IlpGafferMovie)// NOLINT
{
  {
    // clang-format off

   class_<IlpGafferMovie::Startup>("Startup", init())
        .def("initLog", &IlpGafferMovie::Startup::initLog)
        .staticmethod("initLog")
		;

    // clang-format on
  }

  {
    // clang-format off

		scope s = GafferBindings::DependencyNodeClass<IlpGafferMovie::AvReader>()
#if 0
			.def( "setOpenFilesLimit", &OpenImageIOReader::setOpenFilesLimit )
			.staticmethod( "setOpenFilesLimit" )
			.def( "getOpenFilesLimit", &OpenImageIOReader::getOpenFilesLimit )
			.staticmethod( "getOpenFilesLimit" )
#endif
			.def("supportedExtensions", &supportedExtensions<IlpGafferMovie::AvReader>)
			.staticmethod("supportedExtensions")
		;

		enum_<IlpGafferMovie::AvReader::MissingFrameMode>("MissingFrameMode")
			.value("Error", IlpGafferMovie::AvReader::MissingFrameMode::Error)
			.value("Black", IlpGafferMovie::AvReader::MissingFrameMode::Black)
			.value("Hold", IlpGafferMovie::AvReader::MissingFrameMode::Hold)
		;

    // clang-format on
  }

  {
    // clang-format off

    scope s = GafferBindings::DependencyNodeClass<IlpGafferMovie::MovieReader>()
      .def("supportedExtensions", &supportedExtensions<IlpGafferMovie::MovieReader>)
      .staticmethod("supportedExtensions")
      .def("setDefaultColorSpaceFunction", &setDefaultColorSpaceFunction<IlpGafferMovie::MovieReader>)
      .staticmethod("setDefaultColorSpaceFunction")
      .def("getDefaultColorSpaceFunction", &getDefaultColorSpaceFunction<IlpGafferMovie::MovieReader>)
      .staticmethod("getDefaultColorSpaceFunction")
    ;

		enum_<IlpGafferMovie::MovieReader::MissingFrameMode>("MissingFrameMode")
			.value("Error", IlpGafferMovie::MovieReader::MissingFrameMode::Error)
			.value("Black", IlpGafferMovie::MovieReader::MissingFrameMode::Black)
			.value("Hold", IlpGafferMovie::MovieReader::MissingFrameMode::Hold)
		;

    enum_<IlpGafferMovie::MovieReader::FrameMaskMode>("FrameMaskMode")
      .value("None", IlpGafferMovie::MovieReader::FrameMaskMode::None)
      .value("None_", IlpGafferMovie::MovieReader::FrameMaskMode::None)
      .value("BlackOutside", IlpGafferMovie::MovieReader::FrameMaskMode::BlackOutside)
      .value("ClampToFrame", IlpGafferMovie::MovieReader::FrameMaskMode::ClampToFrame)
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
