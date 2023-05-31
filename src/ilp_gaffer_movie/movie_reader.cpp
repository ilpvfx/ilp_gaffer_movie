
#if 0
// This file will be generated automatically when cur_you run the CMake
// configuration step. It creates a namespace called `myproject`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>
#endif

// HACK(tohi): Disable TBB deprecation warning.
#define __TBB_show_deprecation_message_task_scheduler_init_H

#include <ilp_gaffer_movie/movie_reader.hpp>

#include <Gaffer/StringPlug.h>
#include <GafferImage/ImageAlgo.h>
#include <GafferImage/ImagePlug.h>

// #include <GafferImage/ImagePlug.h>

// #include "ilp_gaffer_movie_writer/mux.h"

GAFFER_NODE_DEFINE_TYPE(IlpGafferMovie::MovieReader);

using namespace Gaffer;
using namespace GafferDispatch;
using namespace GafferImage;


namespace IlpGafferMovie {

size_t MovieReader::FirstPlugIndex = 0U;

MovieReader::MovieReader(const std::string &name) : TaskNode(name)
{
  storeIndexOfNextChild(FirstPlugIndex);
  // clang-format off
  addChild(new ImagePlug("in", Plug::In));
  addChild(new StringPlug("fileName"));

  constexpr auto plug_default = static_cast<unsigned int>(Plug::Default);
  constexpr auto plug_serializable = static_cast<unsigned int>(Plug::Serialisable);
  addChild(new ImagePlug("out", Plug::Out, plug_default & ~plug_serializable));
  // clang-format on

  outPlug()->setInput(inPlug());
}

IECore::MurmurHash MovieReader::hash(const Gaffer::Context *context) const
{
  const auto *img_plug = inPlug()->source<ImagePlug>();
  const auto filename = fileNamePlug()->getValue();
  if (filename.empty() || img_plug == inPlug()) { return IECore::MurmurHash(); }

  static_assert(std::is_same_v<std::uintptr_t, uint64_t>);
  const auto img_plug_addr = reinterpret_cast<std::uintptr_t>(img_plug);

  IECore::MurmurHash h = TaskNode::hash(context);
  h.append(static_cast<uint64_t>(img_plug_addr));
  h.append(fileNamePlug()->hash());
  h.append(context->getFrame());
  return h;
}

GafferImage::ImagePlug *MovieReader::inPlug()
{
  constexpr size_t kPlugIndex = 0;
  return getChild<ImagePlug>(FirstPlugIndex + kPlugIndex);
}

const GafferImage::ImagePlug *MovieReader::inPlug() const
{
  constexpr size_t kPlugIndex = 0;
  return getChild<ImagePlug>(FirstPlugIndex + kPlugIndex);
}

Gaffer::StringPlug *MovieReader::fileNamePlug()
{
  constexpr size_t kPlugIndex = 1;
  return getChild<StringPlug>(FirstPlugIndex + kPlugIndex);
}

const Gaffer::StringPlug *MovieReader::fileNamePlug() const
{
  constexpr size_t kPlugIndex = 1;
  return getChild<StringPlug>(FirstPlugIndex + kPlugIndex);
}

GafferImage::ImagePlug *MovieReader::outPlug()
{
  constexpr size_t kPlugIndex = 2;
  return getChild<ImagePlug>(FirstPlugIndex + kPlugIndex);
}

const GafferImage::ImagePlug *MovieReader::outPlug() const
{
  constexpr size_t kPlugIndex = 2;
  return getChild<ImagePlug>(FirstPlugIndex + kPlugIndex);
}

bool MovieReader::requiresSequenceExecution() const { return true; }

void MovieReader::executeSequence(const std::vector<float> &frames) const
{
  IECore::msg(IECore::Msg::Info,
    "MovieReader::executeSequence",
    boost::format("executeSequence \"%d\"") % frames.size());
}

void MovieReader::execute() const
{
  const std::vector<float> frames(1, Context::current()->getFrame());
  executeSequence(frames);
}

}// namespace IlpGafferMovie