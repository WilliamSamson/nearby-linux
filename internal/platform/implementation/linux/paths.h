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

#ifndef PLATFORM_IMPLEMENTATION_LINUX_PATHS_H_
#define PLATFORM_IMPLEMENTATION_LINUX_PATHS_H_

#include <string>

namespace nearby {
namespace linux_impl {

// Returns $HOME, or "/tmp" if HOME is unset (defensive — unlikely on a real
// user session, but unit-test sandboxes can lack HOME).
std::string GetHomeDir();

// Resolves $XDG_DOWNLOAD_DIR via ~/.config/user-dirs.dirs when present.
// Falls back to $HOME/Downloads. Does not create the directory.
std::string GetXdgDownloadDir();

// Resolves $XDG_DATA_HOME, default $HOME/.local/share. Does not create.
std::string GetXdgDataHome();

// Resolves $XDG_CONFIG_HOME, default $HOME/.config. Does not create.
std::string GetXdgConfigHome();

}  // namespace linux_impl
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_LINUX_PATHS_H_
