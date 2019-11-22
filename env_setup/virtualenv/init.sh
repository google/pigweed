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

# Sets up Python 3 virtualenv. Requires PW_ROOT and PW_ENVSETUP to be set,
# which setup.sh (up one level from this file) does.

setup_virtualenv () {
  local venv
  venv="$PW_ROOT/.python3-env"

  local requirements_path
  requirements_path="$PW_ENVSETUP/virtualenv/requirements.txt"

  python3 -m venv "$venv" && \
  . "$venv/bin/activate" && \
  python3 -m pip install --upgrade pip && \
  python3 -m pip install --log "$venv/pip.log" -r "$requirements_path"
}

setup_virtualenv
unset -f setup_virtualenv
