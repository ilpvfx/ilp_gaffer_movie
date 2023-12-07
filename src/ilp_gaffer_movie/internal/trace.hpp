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

#if 0
// NOLINTNEXTLINE
#define TRACE(ctx, msg)                                             \
  do {                                                              \
    if constexpr (IlpGafferMovie::trace_internal::kTrace.enabled) { \
      IlpGafferMovie::trace_internal::TraceImpl(ctx, msg);          \
    }                                                               \
  } while (false)
#endif