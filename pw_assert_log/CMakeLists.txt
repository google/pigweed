# Copyright 2020 The Pigweed Authors
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

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)


add_library(pw_assert_log)
target_sources(pw_assert_log
        PUBLIC assert_log.cc)
target_link_libraries(pw_assert_log
        PUBLIC pw_log
        PRIVATE pw_preprocessor)
target_include_directories(pw_assert_log
        PUBLIC public
        PUBLIC public_overrides)
target_link_libraries(pw_assert_log
        PUBLIC pw_assert.facade)
