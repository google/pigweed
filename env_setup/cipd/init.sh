# Copyright 2019 The Pigweed Authors
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

# Usage: source init.sh
# All this script does is run update.py and then updates PATH. update.py
# outputs the necessary PATH updates to a file and writes 'source <filename>'
# to stdout, so this script only needs to find update.py (next to this file)
# and execute its stdout.

# Sets up Python 3 virtualenv. Requires PW_ENVSETUP to be set, which setup.sh
# (up one level from this file) does.

# Ignore PW_ENVSETUP_FULL here--CIPD setup is extremely fast when doing nothing.

echo "Initializing CIPD..."
$(${PW_ENVSETUP}/cipd/update.py)
