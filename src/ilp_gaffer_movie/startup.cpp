#include "ilp_gaffer_movie/startup.hpp"

#include <mutex>// std::call_once, etc.
#include <string>// std::string

#include "IECore/MessageHandler.h"// IECore::MessageHandler

#include "ilp_movie/log.hpp"// ilp_movie::SetLogLevel, ilp_movie::SetLogCallback

static std::once_flag initLogFlag;

namespace IlpGafferMovie {

void Startup::initLog()
{
  namespace LogLevel = ilp_movie::LogLevel;
  using MessageHandler = IECore::MessageHandler;

  // Could be passed as an argument.
  static const auto kLogLevel = LogLevel::kInfo;

  // Initialize logging so that Gaffer picks up log messages from the ilp_movie function calls.
  std::call_once(initLogFlag, []() {
    ilp_movie::SetLogLevel(kLogLevel);
    ilp_movie::SetLogCallback([](int level, const char *s) {
      auto iecLevel = MessageHandler::Level::Invalid;
      // clang-format off
      switch (level) {
      case LogLevel::kPanic:
      case LogLevel::kFatal:
      case LogLevel::kError:
        iecLevel = MessageHandler::Level::Error; break;
      case LogLevel::kWarning:
        iecLevel = MessageHandler::Level::Warning; break;
      case LogLevel::kInfo:
      case LogLevel::kVerbose:
        iecLevel = MessageHandler::Level::Info; break;
      case LogLevel::kDebug:
      case LogLevel::kTrace:
        iecLevel = MessageHandler::Level::Debug; break;
      case LogLevel::kQuiet: // ???
      default: 
        break;
      }
      // clang-format on

      // Remove trailing newline character since the IECore logger will add one.
      auto str = std::string{ s };
      if (!str.empty() && *str.rbegin() == '\n') { str.erase(str.length() - 1); }

      // Forward the message to Gaffer's logger, with a translated logging level.
      IECore::msg(/*level=*/iecLevel, /*context=*/"ilp_movie", /*message=*/str);
    });
  });
}

}// namespace IlpGafferMovie
