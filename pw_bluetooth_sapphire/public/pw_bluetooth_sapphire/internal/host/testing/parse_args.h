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

#pragma once
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
