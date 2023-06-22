#include <internal/dict_utils.hpp>

// clang-format off
extern "C" {
#include <libavutil/opt.h>
}
// clang-format on

namespace dict_utils_internal {

Options::~Options() noexcept
{
  if (dict != nullptr) {
    av_dict_free(&dict);
    dict = nullptr;
  }
}

auto Count(const Options &opt) noexcept -> int
{
  return opt.dict != nullptr ? av_dict_count(opt.dict) : 0;
}

auto Empty(const Options &opt) noexcept -> bool { return !(Count(opt) > 0); }

auto Entries(const Options &opt) noexcept -> std::vector<std::pair<std::string, std::string>>
{
  if (opt.dict == nullptr) { return {}; }

  // Iterate over all entries.
  std::vector<std::pair<std::string, std::string>> res = {};
  const AVDictionaryEntry *e = nullptr;
  // NOLINTNEXTLINE
  while ((e = av_dict_get(opt.dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
    res.emplace_back(std::make_pair(std::string{ e->key }, std::string{ e->value }));
  }
  return res;
}


}// namespace dict_utils_internal
