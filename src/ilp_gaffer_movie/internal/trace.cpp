#include <internal/trace.hpp>

#include <mutex>

static std::mutex msg_mtx;

namespace IlpGafferMovie {

void TRACE(const std::string &ctx, const std::string &msg) noexcept
{
  if constexpr (trace_internal::kTrace.enabled) {
    std::lock_guard g{ msg_mtx };
    IECore::msg(trace_internal::kTrace.level, ctx, msg);
  }
}

}// namespace IlpGafferMovie
