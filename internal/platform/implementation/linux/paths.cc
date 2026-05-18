// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/linux/paths.h"

#include <cstdlib>
#include <fstream>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace nearby {
namespace linux_impl {

namespace {

std::string EnvOr(const char* name, std::string fallback) {
  const char* v = std::getenv(name);
  if (v && *v) return std::string(v);
  return fallback;
}

// Expands a leading "$HOME" or "~" in `path` against `home`. Stripping is
// minimal — anything not matching is returned untouched.
std::string ExpandHome(absl::string_view path, absl::string_view home) {
  if (absl::ConsumePrefix(&path, "$HOME")) {
    return absl::StrCat(home, path);
  }
  if (absl::ConsumePrefix(&path, "~")) {
    return absl::StrCat(home, path);
  }
  return std::string(path);
}

}  // namespace

std::string GetHomeDir() { return EnvOr("HOME", "/tmp"); }

std::string GetXdgConfigHome() {
  std::string home = GetHomeDir();
  return EnvOr("XDG_CONFIG_HOME", absl::StrCat(home, "/.config"));
}

std::string GetXdgDataHome() {
  std::string home = GetHomeDir();
  return EnvOr("XDG_DATA_HOME", absl::StrCat(home, "/.local/share"));
}

std::string GetXdgDownloadDir() {
  std::string home = GetHomeDir();
  std::string fallback = absl::StrCat(home, "/Downloads");

  // user-dirs.dirs format: lines like
  //   XDG_DOWNLOAD_DIR="$HOME/Downloads"
  std::string user_dirs = absl::StrCat(GetXdgConfigHome(), "/user-dirs.dirs");
  std::ifstream f(user_dirs);
  if (!f.is_open()) return fallback;

  std::string line;
  while (std::getline(f, line)) {
    absl::string_view sv = line;
    if (!absl::ConsumePrefix(&sv, "XDG_DOWNLOAD_DIR=")) continue;
    sv = absl::StripPrefix(sv, "\"");
    sv = absl::StripSuffix(sv, "\"");
    if (sv.empty()) continue;
    return ExpandHome(sv, home);
  }
  return fallback;
}

}  // namespace linux_impl
}  // namespace nearby
