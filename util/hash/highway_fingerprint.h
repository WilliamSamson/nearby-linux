#ifndef UTIL_HASH_HIGHWAY_FINGERPRINT_H_
#define UTIL_HASH_HIGHWAY_FINGERPRINT_H_

#include <cstdint>
#include <string>
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace util_hash {

inline uint64_t HighwayFingerprint64(absl::Span<const uint8_t> data) {
  // Simple fallback hash for compilation.
  uint64_t hash = 0;
  for (uint8_t b : data) {
    hash = hash * 31 + b;
  }
  return hash;
}

inline uint64_t HighwayFingerprint64(absl::string_view data) {
  return HighwayFingerprint64(absl::Span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

}  // namespace util_hash

#endif  // UTIL_HASH_HIGHWAY_FINGERPRINT_H_
