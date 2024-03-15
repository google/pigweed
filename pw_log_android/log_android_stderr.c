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
#include <android/log.h>

// This function will be invoked by libc before main().
// It will configure liblog to direct log messags to stderr rather than logd.
static void __attribute__((constructor)) configure_android_logging(void) {
  __android_log_set_logger(__android_log_stderr_logger);
}
