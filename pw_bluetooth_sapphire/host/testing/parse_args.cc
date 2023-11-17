// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/testing/parse_args.h"

namespace bt::testing {

std::optional<std::string_view> GetArgValue(std::string_view arg_name,
                                            int argc,
                                            char** argv) {
  for (int i = 1; i < argc; i++) {
    std::string_view arg(argv[i]);
    if (arg.substr(0, 2) != "--") {
      continue;
    }
    size_t eq_pos = arg.find("=");
    if (eq_pos == std::string_view::npos ||
        arg.substr(2, arg_name.size()) != arg_name) {
      continue;
    }
    return arg.substr(eq_pos + 1);
  }
  return std::nullopt;
}

}  // namespace bt::testing
