# Copyright 2021 The Pigweed Authors
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
package(default_visibility = ["//visibility:public"])

# --- CPU's ---
alias(
    name = "cortex_m0",
    actual = "@bazel_embedded//platforms:cortex_m0",
)

alias(
    name = "cortex_m1",
    actual = "@bazel_embedded//platforms:cortex_m1",
)

alias(
    name = "cortex_m3",
    actual = "@bazel_embedded//platforms:cortex_m3",
)

alias(
    name = "cortex_m4",
    actual = "@bazel_embedded//platforms:cortex_m4",
)

alias(
    name = "cortex_m4_fpu",
    actual = "@bazel_embedded//platforms:cortex_m4",
)

alias(
    name = "cortex_m7",
    actual = "@bazel_embedded//platforms:cortex_m7",
)

alias(
    name = "cortex_m7_fpu",
    actual = "@bazel_embedded//platforms:cortex_m7_fpu",
)

# --- Chipsets ---
platform(
    name = "stm32f429",
    constraint_values = ["//pw_build/constraints/chipset:stm32f429"],
    parents = [":cortex_m4"],
)

platform(
    name = "lm3s6965evb",
    constraint_values = ["//pw_build/constraints/chipset:lm3s6965evb"],
    parents = [":cortex_m3"],
)

# --- Boards ---
platform(
    name = "stm32f429i-disc1",
    constraint_values = ["//pw_build/constraints/board:stm32f429i-disc1"],
    parents = [":stm32f429"],
)
