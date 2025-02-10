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
"""Common constant values for generated SDK"""

SDK_USER_CONFIG_NAME = "user_config"
SDK_COMMONS_NAME = "commons"

# TODO(krakoczy): Ideally this should be silenced by the downstream project
# revisit this in the future
SDK_DEFAULT_COPTS = [
    "-Wno-cast-qual",
    "-Wno-error=strict-prototypes",
    "-Wno-redundant-decls",
    "-Wno-shadow",
    "-Wno-sign-compare",
    "-Wno-type-limits",
    "-Wno-undef",
    "-Wno-unused-function",
    "-Wno-unused-parameter",
    "-Wno-unused-variable",
]
