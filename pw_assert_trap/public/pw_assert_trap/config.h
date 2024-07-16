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
#pragma once

// Max message buffer size in bytes.
#ifndef PW_ASSERT_TRAP_BUFFER_SIZE
#define PW_ASSERT_TRAP_BUFFER_SIZE 256
#endif  // PW_ASSERT_TRAP_BUFFER_SIZE

// Disable the capture of line, file & function information.  Useful for tests
// or reducing required buffer size.
#ifndef PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE
#define PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE 0
#endif  // PW_ASSERT_TRAP_DISABLE_LOCATION_CAPTURE
