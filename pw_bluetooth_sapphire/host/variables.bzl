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

"""Build information used in the bt-host builds."""

# Note, this file is not copybara'd from upstream Fuchsia.

# LINT.IfChange(copts)
# Common C++ flags used in bt-host to ensure it builds in Pigweed and downstreams.
# Note Fuchsia upstream has its own version of COPTS since this file is not
# copybara'd.
COPTS = [
    # TODO: https://fxbug.dev/345799180 - Remove once code doesn't have unused parameters.
    "-Wno-unused-parameter",
]
# LINT.ThenChange(BUILD.gn:copts)
