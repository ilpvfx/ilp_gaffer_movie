#pragma once

#include <string>// std::string

#include <IECore/MessageHandler.h>

namespace IlpGafferMovie {

namespace trace_internal {
  constexpr struct
  {
    IECore::MessageHandler::Level level = IECore::Msg::Info;
    bool enabled = true;
  } kTrace{};
}// namespace trace_internal

void TRACE(const std::string &ctx, const std::string &msg) noexcept;

}// namespace IlpGafferMovie
