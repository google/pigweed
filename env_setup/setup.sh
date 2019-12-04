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

# This script must be tested on bash, zsh, and dash.

function _realpath () {
  python -c "import os.path; print(os.path.realpath('$@'))"
}

# Shell: bash.
if test -n "$BASH"; then
  PW_SETUP_SCRIPT_PATH=$(_realpath $BASH_SOURCE)
# Shell: zsh.
elif test -n "$ZSH_NAME"; then
  PW_SETUP_SCRIPT_PATH=$(_realpath ${(%):-%N})
# Shell: dash.
elif test ${0##*/} = dash; then
  PW_SETUP_SCRIPT_PATH=$(_realpath \
    $(lsof -p $$ -Fn0 | tail -1 | sed 's#^[^/]*##;'))
# If everything else fails, try $0. It could work.
else
  PW_SETUP_SCRIPT_PATH=$(_realpath $0)
fi

PW_ENVSETUP=$(dirname $PW_SETUP_SCRIPT_PATH)
export PW_ENVSETUP

PW_ROOT=$(dirname "$PW_ENVSETUP")
export PW_ROOT

unset ABORT_PW_ENVSETUP

. "$PW_ENVSETUP/cipd/init.sh"

if [[ -z "$ABORT_PW_ENVSETUP" ]]; then
  . "$PW_ENVSETUP/virtualenv/init.sh"
fi
