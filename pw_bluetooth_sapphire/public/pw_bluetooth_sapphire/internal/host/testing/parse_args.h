// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_PARSE_ARGS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_PARSE_ARGS_H_

#include <charconv>
#include <optional>
#include <string_view>

namespace bt::testing {

// Searches argv for an arg with the name |arg_name|. If the arg is present and
// has a value, the value will be returned.
std::optional<std::string_view> GetArgValue(std::string_view arg_name,
                                            int argc,
                                            char** argv);

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_PARSE_ARGS_H_
