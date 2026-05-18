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
#include <string>

#include "gtest/gtest.h"

namespace nearby {
namespace linux_impl {
namespace {

class PathsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    saved_home_ = GetEnvOr("HOME");
    saved_config_ = GetEnvOr("XDG_CONFIG_HOME");
    saved_data_ = GetEnvOr("XDG_DATA_HOME");
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
  }
  void TearDown() override {
    Restore("HOME", saved_home_);
    Restore("XDG_CONFIG_HOME", saved_config_);
    Restore("XDG_DATA_HOME", saved_data_);
  }
  static std::string GetEnvOr(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
  }
  static void Restore(const char* name, const std::string& value) {
    if (value.empty()) {
      unsetenv(name);
    } else {
      setenv(name, value.c_str(), 1);
    }
  }
  std::string saved_home_;
  std::string saved_config_;
  std::string saved_data_;
};

TEST_F(PathsTest, GetHomeDirHonorsHome) {
  setenv("HOME", "/tmp/qs-test-home", 1);
  EXPECT_EQ(GetHomeDir(), "/tmp/qs-test-home");
}

TEST_F(PathsTest, GetHomeDirFallbackTmpWhenUnset) {
  unsetenv("HOME");
  EXPECT_EQ(GetHomeDir(), "/tmp");
}

TEST_F(PathsTest, GetXdgConfigHomeDefaults) {
  setenv("HOME", "/tmp/qs-test-home", 1);
  EXPECT_EQ(GetXdgConfigHome(), "/tmp/qs-test-home/.config");
}

TEST_F(PathsTest, GetXdgConfigHomeRespectsEnv) {
  setenv("HOME", "/tmp/qs-test-home", 1);
  setenv("XDG_CONFIG_HOME", "/some/path", 1);
  EXPECT_EQ(GetXdgConfigHome(), "/some/path");
}

TEST_F(PathsTest, GetXdgDataHomeDefaults) {
  setenv("HOME", "/tmp/qs-test-home", 1);
  EXPECT_EQ(GetXdgDataHome(), "/tmp/qs-test-home/.local/share");
}

TEST_F(PathsTest, GetXdgDownloadDirFallback) {
  setenv("HOME", "/tmp/qs-test-no-user-dirs", 1);
  // user-dirs.dirs does not exist under that fake HOME -> falls back to
  // $HOME/Downloads.
  EXPECT_EQ(GetXdgDownloadDir(), "/tmp/qs-test-no-user-dirs/Downloads");
}

}  // namespace
}  // namespace linux_impl
}  // namespace nearby
