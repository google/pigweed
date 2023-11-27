# Copyright 2023 The Pigweed Authors
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
include_guard(GLOBAL)

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

set(pw_allocator_POISON_HEAP OFF CACHE STRING
    "When heap poisoning is enabled, a hard-coded randomized pattern will be \
    added before and after the usable space of each Block. The allocator will \
    check that the pattern is unchanged when freeing a block.")

set(pw_allocator_COLLECT_METRICS OFF CACHE STRING
    "Adds a `pw::metric::MerticAccumulation` to `AllocatorProxy`. This \
    increases the code size a non-trivial amount, but allows tracking how much \
    memory each allocator proxy has allocated.")
