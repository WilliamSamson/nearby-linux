// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

#include "internal/platform/implementation/crypto.h"

#include <cstdint>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include <openssl/digest.h>

namespace nearby {
namespace {

ByteArray Hash(absl::string_view input, const EVP_MD* algorithm) {
  if (input.empty()) return {};

  uint8_t digest_buffer[EVP_MAX_MD_SIZE];
  unsigned int digest_size = 0;
  if (!EVP_Digest(input.data(), input.size(), digest_buffer, &digest_size,
                  algorithm, nullptr)) {
    return {};
  }
  return ByteArray(reinterpret_cast<const char*>(digest_buffer), digest_size);
}

}  // namespace

void Crypto::Init() {}

ByteArray Crypto::Md5(absl::string_view input) { return Hash(input, EVP_md5()); }

ByteArray Crypto::Sha256(absl::string_view input) {
  return Hash(input, EVP_sha256());
}

}  // namespace nearby
