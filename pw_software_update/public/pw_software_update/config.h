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

// The size of the buffer to create on stack for streaming manifest data from
// the bundle reader.
#define WRITE_MANIFEST_STREAM_PIPE_BUFFER_SIZE 8

// The maximum allowed length of a target name.
#define MAX_TARGET_NAME_LENGTH 32

// The maximum allowed payload size in bytes. This is used to mitigate DoS
// attacks.
#ifndef PW_SOFTWARE_UPDATE_MAX_TARGET_PAYLOAD_SIZE
#define PW_SOFTWARE_UPDATE_MAX_TARGET_PAYLOAD_SIZE (100 * 1024 * 1024)
#endif  // PW_SOFTWARE_UPDATE_MAX_TARGET_PAYLOAD_SIZE

// Not recommended. Disable compilation of bundle verification.
#ifndef PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION
#define PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION (false)
#endif  // PW_SOFTWARE_UPDATE_DISABLE_BUNDLE_VERIFICATION

// Whether to support bundle "personalization", which is a feature that
// strips some or all target files that a device claims to already have from an
// incoming bundle in order to improve performance.
#ifndef PW_SOFTWARE_UPDATE_WITH_PERSONALIZATION
#define PW_SOFTWARE_UPDATE_WITH_PERSONALIZATION (true)
#endif  // PW_SOFTWARE_UPDATE_WITH_PERSONALIZATION