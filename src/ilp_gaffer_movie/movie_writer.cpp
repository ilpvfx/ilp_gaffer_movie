#include "ilp_gaffer_movie/movie_writer.hpp"

#include <string_view>

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <GafferImage/ImageAlgo.h>

#include "ilp_movie/frame.hpp"
#include "ilp_movie/log.hpp"
#include "ilp_movie/mux.hpp"

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::MovieWriter);

namespace IlpGafferMovie {

size_t MovieWriter::g_FirstPlugIndex = 0U;

MovieWriter::MovieWriter(const std::string &name) : TaskNode(name)
{
  using Plug = Gaffer::Plug;
  using ImagePlug = GafferImage::ImagePlug;
  using IntPlug = Gaffer::IntPlug;
  using StringPlug = Gaffer::StringPlug;
  using ValuePlug = Gaffer::ValuePlug;

  storeIndexOfNextChild(g_FirstPlugIndex);

  addChild(new ImagePlug(// [0]
    /*name=*/"in",
    /*direction=*/Plug::In));
  addChild(new StringPlug(// [1]
    /*name=*/"fileName",
    /*direction=*/Plug::In));
  addChild(new StringPlug(// [2]
    /*name=*/"colorRange",
    /*direction=*/Plug::In,
    /*defaultValue=*/ilp_movie::ColorRange::kPc));
  addChild(new StringPlug(// [3]
    /*name=*/"colorspace",
    /*direction=*/Plug::In,
    /*defaultValue=*/ilp_movie::Colorspace::kBt709));
  addChild(new StringPlug(// [4]
    /*name=*/"colorTrc",
    /*direction=*/Plug::In,
    /*defaultValue=*/ilp_movie::ColorTrc::kIec61966_2_1));
  addChild(new StringPlug(// [5]
    /*name=*/"colorPrimaries",
    /*direction=*/Plug::In,
    /*defaultValue=*/ilp_movie::ColorPrimaries::kBt709));

  addChild(new StringPlug(// [6]
    /*name=*/"filtergraph",
    /*direction=*/Plug::In,
    /*defaultValue=*/
    "scale=in_range=full:in_color_matrix=bt709:out_range=full:out_color_matrix=bt709"
    ":flags=spline+accurate_rnd+full_chroma_int+full_chroma_inp"));

  addChild(new StringPlug(// [7]
    /*name=*/"codec",
    /*direction=*/Plug::In,
    /*defaultValue=*/"prores"));

  // Please the LINTer, it doesn't like bit-wise operations on signed integer types.
  constexpr auto kPlugDefault = static_cast<unsigned int>(Plug::Default);
  constexpr auto kPlugSerializable = static_cast<unsigned int>(Plug::Serialisable);
  addChild(new ImagePlug(// [8]
    /*name=*/"out",
    /*direction=*/Plug::Out,
    /*flags=*/kPlugDefault & ~kPlugSerializable));

  // H264 options.
  auto *h264OptionsPlug = new ValuePlug(/*name=*/"h264", /*direction=*/Plug::In);
  addChild(h264OptionsPlug);// [9]
  h264OptionsPlug->addChild(new StringPlug(// [9][0]
    /*name=*/"preset",
    /*direction=*/Plug::In,
    /*defaultValue=*/"slower"));
  h264OptionsPlug->addChild(new StringPlug(// [9][1]
    /*name=*/"pix_fmt",
    /*direction=*/Plug::In,
    /*defaultValue=*/"yuv420p"));
  h264OptionsPlug->addChild(new StringPlug(// [9][2]
    /*name=*/"tune",
    /*direction=*/Plug::In,
    /*defaultValue=*/"none"));
  h264OptionsPlug->addChild(new IntPlug(// [9][3]
    /*name=*/"crf",
    /*direction=*/Plug::In,
    /*defaultValue=*/23,
    /*minValue=*/0,
    /*maxValue=*/51));
  h264OptionsPlug->addChild(new StringPlug(
    /*name=*/"x264Params",
    /*direction=*/Plug::In,
    /*defaultValue=*/"keyint=15:no-deblock=1"));
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
  auto *proResOptionsPlug = new ValuePlug(/*name=*/"prores", /*direction=*/Plug::In);
  addChild(proResOptionsPlug);// [10]
  proResOptionsPlug->addChild(new StringPlug(// [10][0]
    /*name=*/"profile",
    /*direction=*/Plug::In,
    /*defaultValue=*/"hq_10b"));
  proResOptionsPlug->addChild(new IntPlug(// [10][1]
    /*name=*/"qscale",
    /*direction=*/Plug::In,
    /*defaultValue=*/11,
    /*minValue=*/0,
    /*maxValue=*/32));

  outPlug()->setInput(inPlug());
}

IECore::MurmurHash MovieWriter::hash(const Gaffer::Context *context) const
{
  using ImagePlug = GafferImage::ImagePlug;

  const auto *imgPlug = inPlug()->source<ImagePlug>();
  const auto fileName = fileNamePlug()->getValue();
  if (fileName.empty() || imgPlug == inPlug()) { return IECore::MurmurHash(); }

  static_assert(std::is_same_v<std::uintptr_t, uint64_t>);
  const auto imgPlugAddr = reinterpret_cast<std::uintptr_t>(imgPlug);

  IECore::MurmurHash h = TaskNode::hash(context);
  h.append(static_cast<uint64_t>(imgPlugAddr));
  h.append(fileNamePlug()->hash());
  // h.append(formatPlug()->hash());
  h.append(codecPlug()->hash());
  // h.append(crfPlug()->hash());
  h.append(context->getFrame());
  return h;
}

// clang-format off
// NOLINTNEXTLINE
#define PLUG_MEMBER_IMPL(name, type, index)            \
  type *MovieWriter::name()                            \
  {                                                    \
    return getChild<type>(g_FirstPlugIndex + (index)); \
  }                                                    \
  const type *MovieWriter::name() const                \
  {                                                    \
    return getChild<type>(g_FirstPlugIndex + (index)); \
  }
// clang-format on

PLUG_MEMBER_IMPL(inPlug, GafferImage::ImagePlug, 0U);
PLUG_MEMBER_IMPL(fileNamePlug, Gaffer::StringPlug, 1U);
PLUG_MEMBER_IMPL(colorRangePlug, Gaffer::StringPlug, 2U);
PLUG_MEMBER_IMPL(colorspacePlug, Gaffer::StringPlug, 3U);
PLUG_MEMBER_IMPL(colorTrcPlug, Gaffer::StringPlug, 4U);
PLUG_MEMBER_IMPL(colorPrimariesPlug, Gaffer::StringPlug, 5U);
PLUG_MEMBER_IMPL(filtergraphPlug, Gaffer::StringPlug, 6U);
PLUG_MEMBER_IMPL(codecPlug, Gaffer::StringPlug, 7U);

Gaffer::ValuePlug *MovieWriter::codecSettingsPlug(const std::string &codecName)
{
  return getChild<Gaffer::ValuePlug>(codecName);
}

const Gaffer::ValuePlug *MovieWriter::codecSettingsPlug(const std::string &codecName) const
{
  return getChild<Gaffer::ValuePlug>(codecName);
}

PLUG_MEMBER_IMPL(outPlug, GafferImage::ImagePlug, 8U);

#undef PLUG_MEMBER_IMPL

void MovieWriter::execute() const { executeSequence({ Gaffer::Context::current()->getFrame() }); }

void MovieWriter::executeSequence(const std::vector<float> &frames) const
{
  using Context = Gaffer::Context;
  using ContextPtr = Gaffer::ContextPtr;
  using ImagePlug = GafferImage::ImagePlug;
  using IntPlug = Gaffer::IntPlug;
  using StringPlug = Gaffer::StringPlug;
  using ValuePlug = Gaffer::ValuePlug;
  using namespace std::literals;

  IECore::msg(IECore::Msg::Info,
    "MovieWriter::executeSequence",
    boost::format("executeSequence \"%d\"") % frames.size());

  if (frames.empty()) { throw IECore::Exception("No input frames"); }

  const auto *imgPlug = inPlug()->getInput<ImagePlug>();
  if (imgPlug == nullptr) { throw IECore::Exception("No input image"); }

  const std::string codec = codecPlug()->getValue();
  const ValuePlug *codec_settings = codecSettingsPlug(codec);
  if (codec_settings == nullptr) { throw IECore::Exception("Unknown codec"); }

  // SceneInterfacePtr output;
  // tbb::mutex mutex;
  ContextPtr context = new Context(*Context::current());
  Context::Scope scopedContext(context.get());

  // Setup muxer.
  //
  // NOTE(tohi): We cannot set the width and height until we have "seen" the first image.
  const auto make_mux_ctx = [&](const int width, const int height) {
    const auto filename = fileNamePlug()->getValue();
    const auto frame_rate = static_cast<double>(context->getFramesPerSecond());

    // TODO(tohi): H264 only?!
    const auto color_range = colorRangePlug()->getValue();
    const auto colorspace = colorspacePlug()->getValue();
    const auto color_primaries = colorPrimariesPlug()->getValue();
    const auto color_trc = colorTrcPlug()->getValue();

    ilp_movie::MuxParameters mux_params = {};
    if (codec == "h264") {
      const auto pix_fmt = codec_settings->getChild<StringPlug>("pix_fmt")->getValue();
      const auto preset = codec_settings->getChild<StringPlug>("preset")->getValue();
      const auto tune = codec_settings->getChild<StringPlug>("tune")->getValue();
      const auto x264_params = codec_settings->getChild<StringPlug>("x264Params")->getValue();

      ilp_movie::H264::EncodeParameters enc_params = {};
      enc_params.pix_fmt = pix_fmt;
      enc_params.preset = preset;
      if (tune != "none") { enc_params.tune = tune; }
      enc_params.crf = codec_settings->getChild<IntPlug>("crf")->getValue();
      enc_params.x264_params = x264_params;
      enc_params.color_range = color_range;
      enc_params.colorspace = colorspace;
      enc_params.color_primaries = color_primaries;
      enc_params.color_trc = color_trc;
      auto mp = ilp_movie::MakeMuxParameters(filename,
        /*width=*/width,
        /*height=*/height,
        /*frame_rate=*/frame_rate,
        enc_params);
      if (!mp.has_value()) { throw IECore::Exception("Bad H264 parameters"); }
      mux_params = std::move(*mp);
    } else if (codec == "prores") {
      ilp_movie::ProRes::EncodeParameters enc_params = {};
      enc_params.qscale = codec_settings->getChild<IntPlug>("qscale")->getValue();
      // clang-format off
      const auto p_str = codec_settings->getChild<StringPlug>("profile")->getValue();
      if      (p_str == "proxy_10b"sv    ) { enc_params.profile_name = ilp_movie::ProRes::ProfileName::kProxy;    }
      else if (p_str == "lt_10b"sv       ) { enc_params.profile_name = ilp_movie::ProRes::ProfileName::kLt;       }
      else if (p_str == "standard_10b"sv ) { enc_params.profile_name = ilp_movie::ProRes::ProfileName::kStandard; }
      else if (p_str == "hq_10b"sv       ) { enc_params.profile_name = ilp_movie::ProRes::ProfileName::kHq;       }
      else if (p_str == "4444_10b"sv     ) { enc_params.profile_name = ilp_movie::ProRes::ProfileName::k4444;     }
      else if (p_str == "4444xq_10b"sv   ) { enc_params.profile_name = ilp_movie::ProRes::ProfileName::k4444xq;   }
      else { throw IECore::Exception("Bad ProRes profile / pixel format"); }
      // clang-format on
      auto mp = ilp_movie::MakeMuxParameters(filename,
        /*width=*/width,
        /*height=*/height,
        /*frame_rate=*/frame_rate,
        enc_params);
      if (!mp.has_value()) { throw IECore::Exception("Bad ProRes parameters"); }
      mux_params = std::move(*mp);
    } else {
      throw IECore::Exception("Unsupported codec");
    }

    mux_params.filter_graph = filtergraphPlug()->getValue();

    return ilp_movie::MakeMuxContext(mux_params);
  };

  std::unique_ptr<ilp_movie::MuxContext> mux_ctx = nullptr;
  for (const float frame : frames) {
    IECore::msg(
      IECore::Msg::Info, "MovieWriter", boost::format("Encode frame %d") % static_cast<int>(frame));

    context->setFrame(frame);
    const auto imgPtr = GafferImage::ImageAlgo::image(imgPlug, /*viewName=*/nullptr);
    if (imgPtr == nullptr) { throw IECore::Exception("Bad image pointer"); }

    const auto displayWindow = imgPtr->getDisplayWindow();
    const int width = displayWindow.size().x + 1;
    const int height = displayWindow.size().y + 1;
    if (mux_ctx == nullptr) {
      mux_ctx = make_mux_ctx(width, height);
      if (mux_ctx == nullptr) { throw IECore::Exception("Mux init failed"); }
    }

    // TODO(tohi): Only send pixels in display window?

    // readable?
    auto &&r_ch = imgPtr->getChannel<float>(GafferImage::ImageAlgo::channelNameR);
    auto &&g_ch = imgPtr->getChannel<float>(GafferImage::ImageAlgo::channelNameG);
    auto &&b_ch = imgPtr->getChannel<float>(GafferImage::ImageAlgo::channelNameB);
    if (r_ch == nullptr || g_ch == nullptr || b_ch == nullptr) {
      throw IECore::Exception("Cannot get RGB channels");
    }

    // Pass a frame (view) to the muxer.
    ilp_movie::FrameView frameView{};
    frameView.hdr.width = width;
    frameView.hdr.height = height;
    // frameView.hdr.pixel_aspect_ratio = ;
    frameView.hdr.frame_nb = static_cast<int>(frame);
    frameView.hdr.pix_fmt_name = ilp_movie::PixFmt::kRGB_P_F32;
    frameView.hdr.color_range_name = ilp_movie::ColorRange::kPc;
    // frameView.hdr.color_space_name = ;
    // frameView.hdr.color_trc_name = ;
    // frameView.hdr.color_primaries_name = ;

    // Manually set up data pointers for the color planes. In this case the
    // data pointers don't point into the view's buffer, which is technically
    // not required, but it means that we have to do things manually.
    frameView.data[0] = reinterpret_cast<uint8_t *>(g_ch->writable().data());
    frameView.data[1] = reinterpret_cast<uint8_t *>(b_ch->writable().data());
    frameView.data[2] = reinterpret_cast<uint8_t *>(r_ch->writable().data());
    frameView.data[3] = nullptr;
    frameView.linesize[0] = width * static_cast<int>(sizeof(float));
    frameView.linesize[1] = width * static_cast<int>(sizeof(float));
    frameView.linesize[2] = width * static_cast<int>(sizeof(float));
    frameView.linesize[3] = 0;

    if (!ilp_movie::MuxWriteFrame(*mux_ctx, frameView)) {
      throw IECore::Exception("Mux write frame failed");
    }

    // imgPtr->ilp_movie::MuxFrame mux_frame = {};
    // mux_frame.width = mux_ctx->params.width;
    // mux_frame.height = mux_ctx->params.height;
    // mux_frame.frame_nb = frame;
    // mux_frame.r = imgPtr->getChannel<float>("R")->readable().data();
    // mux_frame.g = imgPtr->getChannel<float>("G")->readable().data();
    // mux_frame.b = imgPtr->getChannel<float>("B")->readable().data();
  }

  if (!ilp_movie::MuxFinish(*mux_ctx)) { throw IECore::Exception("Mux finish failed"); }
  IECore::msg(IECore::Msg::Info, "MovieWriter", boost::format("Encode finished"));
}

}// namespace IlpGafferMovie


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
