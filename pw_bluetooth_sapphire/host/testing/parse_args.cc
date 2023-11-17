// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
