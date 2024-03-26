// Copyright 2024 The Pigweed Authors
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

#define PW_LOG_MODULE_NAME "PW_BID"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_build_info/util.h"

#include "pw_build_info/build_id.h"
#include "pw_log/log.h"
#include "pw_string/string_builder.h"

namespace pw::build_info {

void LogBuildId() {
  StringBuffer<kMaxBuildIdSizeBytes * 2 + 1> sb;
  auto build_id = BuildId();
  sb << build_id;
  PW_LOG_INFO("Build ID: %s", sb.data());
}

}  // namespace pw::build_info
