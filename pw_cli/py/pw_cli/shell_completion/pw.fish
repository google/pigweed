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

# This script must be tested on fish 3.6.0

set _pw_fish_completion_path (path resolve (status current-dirname)/fish)

# Add the fish subdirectory to the completion path
if not contains $_pw_fish_completion_path $fish_complete_path
    set -x --append fish_complete_path $_pw_fish_completion_path
end
