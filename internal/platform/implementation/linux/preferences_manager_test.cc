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

#include "internal/platform/implementation/linux/preferences_manager.h"

#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"

namespace nearby {
namespace linux_impl {
namespace {

class PreferencesManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char tmpl[] = "/tmp/qs_prefs_XXXXXX";
    ASSERT_NE(mkdtemp(tmpl), nullptr) << strerror(errno);
    dir_ = FilePath(std::string(tmpl));
  }
  void TearDown() override {
    // best-effort cleanup; not critical if leaves files in /tmp.
    Files::RemoveFile(dir_.append(FilePath("settings.json")));
    Files::RemoveDirectory(dir_);
  }
  FilePath dir_;
};

TEST_F(PreferencesManagerTest, BooleanRoundtrip) {
  PreferencesManager prefs(dir_);
  EXPECT_FALSE(prefs.GetBoolean("flag", false));
  EXPECT_TRUE(prefs.SetBoolean("flag", true));
  EXPECT_TRUE(prefs.GetBoolean("flag", false));
}

TEST_F(PreferencesManagerTest, StringRoundtrip) {
  PreferencesManager prefs(dir_);
  EXPECT_EQ(prefs.GetString("name", "default"), "default");
  EXPECT_TRUE(prefs.SetString("name", "ubuntu"));
  EXPECT_EQ(prefs.GetString("name", "default"), "ubuntu");
}

TEST_F(PreferencesManagerTest, PersistsAcrossInstances) {
  {
    PreferencesManager a(dir_);
    EXPECT_TRUE(a.SetString("name", "persistent"));
    EXPECT_TRUE(a.SetInt64("count", 42));
  }
  PreferencesManager b(dir_);
  EXPECT_EQ(b.GetString("name", ""), "persistent");
  EXPECT_EQ(b.GetInt64("count", 0), 42);
}

TEST_F(PreferencesManagerTest, RemoveDropsKey) {
  PreferencesManager prefs(dir_);
  prefs.SetString("name", "to-remove");
  ASSERT_EQ(prefs.GetString("name", "missing"), "to-remove");
  prefs.Remove("name");
  EXPECT_EQ(prefs.GetString("name", "missing"), "missing");
}

TEST_F(PreferencesManagerTest, RemoveKeyPrefixDropsMatching) {
  PreferencesManager prefs(dir_);
  prefs.SetString("ui.theme", "dark");
  prefs.SetString("ui.font", "mono");
  prefs.SetString("device.name", "ubuntu");
  EXPECT_TRUE(prefs.RemoveKeyPrefix("ui."));
  EXPECT_EQ(prefs.GetString("ui.theme", ""), "");
  EXPECT_EQ(prefs.GetString("ui.font", ""), "");
  EXPECT_EQ(prefs.GetString("device.name", ""), "ubuntu");
}

}  // namespace
}  // namespace linux_impl
}  // namespace nearby
