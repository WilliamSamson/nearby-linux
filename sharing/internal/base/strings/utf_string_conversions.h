#ifndef SHARING_INTERNAL_BASE_STRINGS_UTF_STRING_CONVERSIONS_H_
#define SHARING_INTERNAL_BASE_STRINGS_UTF_STRING_CONVERSIONS_H_

#include <string>
#include <string_view>
#include <glib.h>

namespace nearby::utils {

inline bool IsStringUtf8(std::string_view str) {
  return g_utf8_validate(str.data(), str.size(), nullptr);
}

inline void TruncateUtf8ToByteSize(const std::string& input, size_t byte_size,
                             std::string* output) {
  if (input.size() <= byte_size) {
    *output = input;
    return;
  }
  
  const char* end = g_utf8_find_prev_char(input.data() + byte_size + 1, input.data() + byte_size);
  if (end) {
    *output = std::string(input.data(), end - input.data());
  } else {
    *output = "";
  }
}

}  // namespace nearby::utils

#endif  // SHARING_INTERNAL_BASE_STRINGS_UTF_STRING_CONVERSIONS_H_
