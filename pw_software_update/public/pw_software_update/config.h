// Copyright 2021 The Pigweed Authors
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

// The log level to use for this module. Logs below this level are omitted.
#define PW_LOG_MODULE_NAME "PWSU"
#ifndef PW_SOFTWARE_UPDATE_CONFIG_LOG_LEVEL
#define PW_SOFTWARE_UPDATE_CONFIG_LOG_LEVEL PW_LOG_LEVEL_ERROR
#endif  // PW_SOFTWARE_UPDATE_CONFIG_LOG_LEVEL

// The size of the buffer to create on stack for streaming manifest data from
// the bundle reader.
#define WRITE_MANIFEST_STREAM_PIPE_BUFFER_SIZE 8

// The maximum allowed length of a target name.
#define MAX_TARGET_NAME_LENGTH 32

// Not recommended. Disable compilation of bundle verification.
#ifndef PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION
#define PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION (false)
#endif  // PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION
