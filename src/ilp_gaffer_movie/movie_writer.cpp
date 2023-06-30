#include <ilp_gaffer_movie/movie_writer.hpp>

#include <string_view>

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <Gaffer/StringPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImagePlug.h>

// #include <GafferImage/ImagePlug.h>

#include <ilp_movie/log.hpp>
#include <ilp_movie/mux.hpp>

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::MovieWriter);

using namespace Gaffer;
using namespace GafferDispatch;
using namespace GafferImage;

namespace {
struct ProfilePixelFormat
{
  const char *profile = nullptr;
  const char *pix_fmt = nullptr;
};
}// namespace

namespace IlpGafferMovie {

size_t MovieWriter::FirstPlugIndex = 0U;

MovieWriter::MovieWriter(const std::string &name) : TaskNode(name)
{
  storeIndexOfNextChild(FirstPlugIndex);
  // clang-format off
  addChild(new ImagePlug("in", Plug::In));
  addChild(new StringPlug("fileName"));
  //addChild(new StringPlug("format", Plug::In, "mov"));
  addChild(new StringPlug("colorRange", Plug::In, "pc"));
  addChild(new StringPlug("colorspace", Plug::In, "bt709"));
  addChild(new StringPlug("colorPrimaries", Plug::In, "bt709"));
  addChild(new StringPlug("colorTrc", Plug::In, "iec61966-2-1"));
  addChild(new StringPlug("swsFlags", Plug::In, 
    "flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp"));
  addChild(new StringPlug("filtergraph", Plug::In,
    "scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709"));
  addChild(new StringPlug("codec", Plug::In, "prores"));

  constexpr auto plug_default = static_cast<unsigned int>(Plug::Default);
  constexpr auto plug_serializable = static_cast<unsigned int>(Plug::Serialisable);
  addChild(new ImagePlug("out", Plug::Out, plug_default & ~plug_serializable));
  // clang-format on

  outPlug()->setInput(inPlug());

  // H264 options.
  auto *h264OptionsPlug = new ValuePlug("h264");
  addChild(h264OptionsPlug);
  h264OptionsPlug->addChild(new StringPlug("preset", Plug::In, "slower"));
  h264OptionsPlug->addChild(new StringPlug("pix_fmt", Plug::In, "yuv420p"));
  h264OptionsPlug->addChild(new StringPlug("tune", Plug::In, "none"));
  h264OptionsPlug->addChild(
    new IntPlug("crf", Plug::In, /*defaultValue=*/23, /*minValue=*/0, /*maxValue=*/51));
  h264OptionsPlug->addChild(new StringPlug("x264Params", Plug::In, "keyint=15:no-deblock=1"));
#if 0
  ValuePlug *h264AdvancedOptionsPlug = new ValuePlug("h264Advanced");
  addChild(h264AdvancedOptionsPlug);
  h264AdvancedOptionsPlug->addChild(new IntPlug("bFrames", Plug::In, 0, /*minValue=*/0));
  h264AdvancedOptionsPlug->addChild(new IntPlug("gopSize", Plug::In, 12, /*minValue=*/0));
  h264AdvancedOptionsPlug->addChild(new IntPlug("bitrate", Plug::In, 28000, /*minValue=*/0));
  h264AdvancedOptionsPlug->addChild(new IntPlug("bitrateTolerance", Plug::In, 0, /*minValue=*/0));
  h264AdvancedOptionsPlug->addChild(new IntPlug("qmin", Plug::In, 1, /*minValue=*/0));    
  h264AdvancedOptionsPlug->addChild(new IntPlug("qmax", Plug::In, 3, /*minValue=*/0));
#endif

  // ProRes options.
  auto *proResOptionsPlug = new ValuePlug("prores");
  addChild(proResOptionsPlug);
  proResOptionsPlug->addChild(new StringPlug("profile", Plug::In, "hq_10b"));
  proResOptionsPlug->addChild(new IntPlug("qscale", Plug::In, 11, /*minValue=*/0, /*maxValue=*/32));
}

IECore::MurmurHash MovieWriter::hash(const Gaffer::Context *context) const
{
  const auto *img_plug = inPlug()->source<ImagePlug>();
  const auto filename = fileNamePlug()->getValue();
  if (filename.empty() || img_plug == inPlug()) { return IECore::MurmurHash(); }

  static_assert(std::is_same_v<std::uintptr_t, uint64_t>);
  const auto img_plug_addr = reinterpret_cast<std::uintptr_t>(img_plug);

  IECore::MurmurHash h = TaskNode::hash(context);
  h.append(static_cast<uint64_t>(img_plug_addr));
  h.append(fileNamePlug()->hash());
  // h.append(formatPlug()->hash());
  h.append(codecPlug()->hash());
  // h.append(crfPlug()->hash());
  h.append(context->getFrame());
  return h;
}

GafferImage::ImagePlug *MovieWriter::inPlug() { return getChild<ImagePlug>(FirstPlugIndex); }

const GafferImage::ImagePlug *MovieWriter::inPlug() const
{
  return getChild<ImagePlug>(FirstPlugIndex);
}

Gaffer::StringPlug *MovieWriter::fileNamePlug() { return getChild<StringPlug>(FirstPlugIndex + 1); }

const Gaffer::StringPlug *MovieWriter::fileNamePlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 1);
}

#if 0
Gaffer::StringPlug *MovieWriterSequential::formatPlug() {
  return getChild<StringPlug>(FirstPlugIndex + );
}

const Gaffer::StringPlug *MovieWriterSequential::formatPlug() const {
  return getChild<StringPlug>(FirstPlugIndex + );
}
#endif

Gaffer::StringPlug *MovieWriter::colorRangePlug()
{
  return getChild<StringPlug>(FirstPlugIndex + 2);
}
const Gaffer::StringPlug *MovieWriter::colorRangePlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 2);
}

Gaffer::StringPlug *MovieWriter::colorspacePlug()
{
  return getChild<StringPlug>(FirstPlugIndex + 3);
}
const Gaffer::StringPlug *MovieWriter::colorspacePlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 3);
}

Gaffer::StringPlug *MovieWriter::colorPrimariesPlug()
{
  return getChild<StringPlug>(FirstPlugIndex + 4);
}
const Gaffer::StringPlug *MovieWriter::colorPrimariesPlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 4);
}

Gaffer::StringPlug *MovieWriter::colorTrcPlug() { return getChild<StringPlug>(FirstPlugIndex + 5); }
const Gaffer::StringPlug *MovieWriter::colorTrcPlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 5);
}

Gaffer::StringPlug *MovieWriter::swsFlagsPlug() { return getChild<StringPlug>(FirstPlugIndex + 6); }
const Gaffer::StringPlug *MovieWriter::swsFlagsPlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 6);
}

Gaffer::StringPlug *MovieWriter::filtergraphPlug()
{
  return getChild<StringPlug>(FirstPlugIndex + 7);
}
const Gaffer::StringPlug *MovieWriter::filtergraphPlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 7);
}

Gaffer::StringPlug *MovieWriter::codecPlug() { return getChild<StringPlug>(FirstPlugIndex + 8); }

const Gaffer::StringPlug *MovieWriter::codecPlug() const
{
  return getChild<StringPlug>(FirstPlugIndex + 8);
}

GafferImage::ImagePlug *MovieWriter::outPlug() { return getChild<ImagePlug>(FirstPlugIndex + 9); }

const GafferImage::ImagePlug *MovieWriter::outPlug() const
{
  return getChild<ImagePlug>(FirstPlugIndex + 9);
}

Gaffer::ValuePlug *MovieWriter::codecSettingsPlug(const std::string &codec_name)
{
  return getChild<ValuePlug>(codec_name);
}

const Gaffer::ValuePlug *MovieWriter::codecSettingsPlug(const std::string &codec_name) const
{
  return getChild<ValuePlug>(codec_name);
}

bool MovieWriter::requiresSequenceExecution() const { return true; }

void MovieWriter::executeSequence(const std::vector<float> &frames) const
{
  using namespace std::literals;

  IECore::msg(IECore::Msg::Info,
    "MovieWriter::executeSequence",
    boost::format("executeSequence \"%d\"") % frames.size());

  const auto *img = inPlug()->getInput<ImagePlug>();
  if (img == nullptr) { throw IECore::Exception("No input image"); }

  const std::string codec = codecPlug()->getValue();
  const ValuePlug *codec_settings = codecSettingsPlug(codec);
  if (codec_settings == nullptr) { throw IECore::Exception("Unknown codec"); }

  // SceneInterfacePtr output;
  // tbb::mutex mutex;
  ContextPtr context = new Context(*Context::current());
  Context::Scope scopedContext(context.get());

  const std::string filename = fileNamePlug()->getValue();
  const std::string color_range = colorRangePlug()->getValue();
  const std::string colorspace = colorspacePlug()->getValue();
  const std::string color_primaries = colorPrimariesPlug()->getValue();
  const std::string color_trc = colorTrcPlug()->getValue();
  const std::string sws_flags = swsFlagsPlug()->getValue();
  const std::string filtergraph = filtergraphPlug()->getValue();
  std::string preset;
  std::string pix_fmt;
  std::string tune;
  std::string x264_params;

  // Setup muxer.
  //
  // NOTE(tohi): We cannot set the width and height until we have "seen" the first image.
  ilp_movie::SetLogLevel(ilp_movie::LogLevel::kInfo);
  ilp_movie::SetLogCallback([](int level, const char *s) {
    auto iec_level = IECore::MessageHandler::Level::Invalid;
    // clang-format off
    switch (level) {
    case ilp_movie::LogLevel::kPanic:
    case ilp_movie::LogLevel::kFatal:
    case ilp_movie::LogLevel::kError:
      iec_level = IECore::MessageHandler::Level::Error; break;
    case ilp_movie::LogLevel::kWarning:
      iec_level = IECore::MessageHandler::Level::Warning; break;
    case ilp_movie::LogLevel::kInfo:
    case ilp_movie::LogLevel::kVerbose:
    case ilp_movie::LogLevel::kDebug:
    case ilp_movie::LogLevel::kTrace:
      iec_level = IECore::MessageHandler::Level::Info; break;
    case ilp_movie::LogLevel::kQuiet: // ???
    default: 
      break;
    }
    // clang-format on

    // Remove trailing newline character since the IECore logger will add one.
    auto str = std::string{ s };
    if (!str.empty() && str[str.length() - 1] == '\n') { str.erase(str.length() - 1); }
    IECore::msg(iec_level, "MovieWriter", str);
  });

  ilp_movie::MuxParameters mux_params = {};
  mux_params.filename = filename.c_str();
  mux_params.fps = static_cast<double>(context->getFramesPerSecond());
  mux_params.mp4_metadata.color_range = color_range.c_str();
  mux_params.mp4_metadata.colorspace = colorspace.c_str();
  mux_params.mp4_metadata.color_primaries = color_primaries.c_str();
  mux_params.mp4_metadata.color_trc = color_trc.c_str();
  mux_params.sws_flags = sws_flags.c_str();
  mux_params.filtergraph = filtergraph.c_str();
  std::unique_ptr<ilp_movie::MuxContext> mux_ctx = nullptr;

  if (codec == "h264") {
    preset = codec_settings->getChild<StringPlug>("preset")->getValue();
    pix_fmt = codec_settings->getChild<StringPlug>("pix_fmt")->getValue();
    tune = codec_settings->getChild<StringPlug>("tune")->getValue();
    x264_params = codec_settings->getChild<StringPlug>("x264Params")->getValue();

    ilp_movie::h264::Config cfg = {};
    cfg.preset = preset.c_str();
    cfg.pix_fmt = pix_fmt.c_str();
    if (tune != "none") { mux_params.h264.cfg.tune = tune.c_str(); }
    mux_params.h264.cfg.crf = codec_settings->getChild<IntPlug>("crf")->getValue();
    mux_params.h264.cfg.x264_params = x264_params.c_str();
    //mux_ctx.h264.qp = codec_settings->getChild<IntPlug>("qp")->getValue();

    mux_params.codec_name = "libx264";
    mux_params.h264.cfg = cfg;
  } else if (codec == "prores") {
    // clang-format off
    ilp_movie::ProRes::Config cfg = {};
    const auto p_str = codec_settings->getChild<StringPlug>("profile")->getValue();
    if      (p_str == "proxy_10b"sv    ) { cfg = ilp_movie::ProRes::Config::Preset("proxy").value();    }
    else if (p_str == "lt_10b"sv       ) { cfg = ilp_movie::ProRes::Config::Preset("lt").value();       }
    else if (p_str == "standard_10b"sv ) { cfg = ilp_movie::ProRes::Config::Preset("standard").value(); }
    else if (p_str == "hq_10b"sv       ) { cfg = ilp_movie::ProRes::Config::Preset("hq").value();       }
    else if (p_str == "4444_10b"sv     ) { cfg = ilp_movie::ProRes::Config::Preset("4444").value();     }
    else if (p_str == "4444xq_10b"sv   ) { cfg = ilp_movie::ProRes::Config::Preset("4444xq").value();   }
    else { throw IECore::Exception("Bad ProRes profile / pixel format"); }
    // clang-format on

    cfg.qscale = codec_settings->getChild<IntPlug>("qscale")->getValue();

    mux_params.codec_name = "prores_ks";
    mux_params.pro_res.cfg = cfg;
  } else {
    throw IECore::Exception("Unsupported codec");
  }

#if 0
  mux_ctx.gop_size = 12;
  mux_ctx.bit_rate = 400000;
  mux_ctx.frame_rate = 25;
#endif

  for (const float frame : frames) {
    // IECore::msg(IECore::Msg::Info, "MovieWriterSequential::mux", boost::format("frame: %f") %
    // frame);
    context->setFrame(frame);
    const auto img_ptr = ImageAlgo::image(img);

    if (mux_ctx == nullptr) {
      mux_params.width = img_ptr->getDisplayWindow().size().x + 1;
      mux_params.height = img_ptr->getDisplayWindow().size().y + 1;
      mux_ctx = ilp_movie::MakeMuxContext(mux_params);
      if (mux_ctx == nullptr) { throw IECore::Exception("Mux init failed"); }
    }

    // TODO(tohi): Only send pixels in display window?

    // Pass a frame to the muxer.
    ilp_movie::MuxFrame mux_frame = {};
    mux_frame.width = mux_ctx->params.width;
    mux_frame.height = mux_ctx->params.height;
    mux_frame.frame_nb = frame;
    mux_frame.r = img_ptr->getChannel<float>("R")->readable().data();
    mux_frame.g = img_ptr->getChannel<float>("G")->readable().data();
    mux_frame.b = img_ptr->getChannel<float>("B")->readable().data();
    if (!ilp_movie::MuxWriteFrame(*mux_ctx, mux_frame)) {
      throw IECore::Exception("Mux write frame failed");
    }
  }

  if (!ilp_movie::MuxFinish(*mux_ctx)) {
    throw IECore::Exception("Mux finish failed");
  }

#if 0
    ConstCompoundDataPtr sets;
    bool useSetsAPI = true;
    const std::string fileName = fileNamePlug()->getValue();
    if (!output || output->fileName() != fileName) {
      createDirectories(fileName);
      output = SceneInterface::create(fileName, IndexedIO::Write);
      sets = SceneAlgo::sets(scene);
      useSetsAPI = SceneReader::useSetsAPI(output.get());
    }
#endif

#if 0
    LocationWriter locationWriter(output, !useSetsAPI ? sets : nullptr, context->getTime(), mutex);
    SceneAlgo::parallelProcessLocations(scene, locationWriter);

    if (useSetsAPI && sets) {
      for (const auto &[name, data] : sets->readable()) {
        output->writeSet(name, static_cast<const PathMatcherData *>(data.get())->readable());
      }
    }
  }
#endif
}

void MovieWriter::execute() const
{
  // const std::vector<float> frames(1, Context::current()->getFrame());
  // executeSequence(frames);
  executeSequence({ Context::current()->getFrame() });
}

}// namespace IlpGafferMovie