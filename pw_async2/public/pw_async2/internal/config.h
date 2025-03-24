// Copyright 2025 The Pigweed Authors
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

// Configuration macros for the pw_async2 module.
#pragma once

/// The log level to use for this module. Logs below this level are omitted.
#ifndef PW_ASYNC2_CONFIG_LOG_LEVEL
#define PW_ASYNC2_CONFIG_LOG_LEVEL PW_LOG_LEVEL_INFO
#endif  // PW_ASYNC2_CONFIG_LOG_LEVEL

/// The log module name to use for this module.
#ifndef PW_ASYNC2_CONFIG_LOG_MODULE_NAME
#define PW_ASYNC2_CONFIG_LOG_MODULE_NAME "PW_ASYNC2"
#endif  // PW_ASYNC2_CONFIG_LOG_MODULE_NAME
