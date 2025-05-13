# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Standard kernel flags which can be used by the various platform definitions..
"""

KERNEL_COMMON_FLAGS = {
    # Default to using the tokenized backend.  Platforms can overwrite this value
    "@pigweed//pw_log/rust:pw_log_backend": "//pw_kernel/subsys/console:pw_log_backend_tokenized",
    "@pigweed//pw_toolchain:cortex-m_toolchain_kind": "clang",
    # For now, enable debug assertions for all builds
    "@rules_rust//rust/settings:extra_rustc_flag": "-Cdebug-assertions",
    "@rules_rust//rust/toolchain/channel": "nightly",
}
