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

function _abspath () {
  python -c "import os.path; print(os.path.abspath('$@'))"
}

# Shell: bash.
if test -n "$BASH"; then
  PW_SETUP_SCRIPT_PATH=$(_abspath $BASH_SOURCE)
# Shell: zsh.
elif test -n "$ZSH_NAME"; then
  PW_SETUP_SCRIPT_PATH=$(_abspath ${(%):-%N})
# Shell: dash.
elif test ${0##*/} = dash; then
  PW_SETUP_SCRIPT_PATH=$(_abspath \
    $(lsof -p $$ -Fn0 | tail -1 | sed 's#^[^/]*##;'))
# If everything else fails, try $0. It could work.
else
  PW_SETUP_SCRIPT_PATH=$(_abspath $0)
fi

PW_ENVSETUP=$(dirname $PW_SETUP_SCRIPT_PATH)
export PW_ENVSETUP

PW_ROOT=$(dirname "$PW_ENVSETUP")
export PW_ROOT

unset ABORT_PW_ENVSETUP
unset PW_ENVSETUP_FULL

if [[ $(basename $PW_SETUP_SCRIPT_PATH) == "bootstrap.sh" ]]; then
  PW_ENVSETUP_FULL=1
  export PW_ENVSETUP_FULL
fi

_pw_run_step () {
  NAME=$1
  CMD=$2
  echo -n "Setting up $NAME..."

  TMP=$(mktemp "/tmp/pigweed-envsetup-$NAME.XXXXXX")
  eval $CMD &> $TMP
  if [[ "$?" -ne 0 ]]; then
    ABORT_PW_ENVSETUP=1
  fi

  if [[ -z "$ABORT_PW_ENVSETUP" ]]; then
    echo "done."
  else
    echo "FAILED."
    echo "$NAME setup output:"
    cat $TMP
  fi
}

# Initialize CIPD.
_pw_run_step "cipd" ". $PW_ENVSETUP/cipd/init.sh"

# Check that Python 3 comes from CIPD.
if [[ -z "$ABORT_PW_ENVSETUP" ]]; then
  if [[ $(which python3) != *"cipd"* ]]; then
    echo "Python not from CIPD--found $(which python3) instead" 1>&2
    ABORT_PW_ENVSETUP=1
  fi
fi

# Initialize Python 3 virtual environment.
if [[ -z "$ABORT_PW_ENVSETUP" ]]; then
  _pw_run_step "python" ". $PW_ENVSETUP/virtualenv/init.sh"
fi

# Do an initial host build.
if [[ -z "$ABORT_PW_ENVSETUP" ]]; then
  _pw_run_step "host_tools" ". $PW_ENVSETUP/host_build/init.sh"
fi

if [[ -n "$ABORT_PW_ENVSETUP" ]]; then
  echo "Environment setup failed! Please see messages above." 1>&2
fi

# Unset this at the end so users running setup scripts directly get consistent
# behavior.
unset PW_ENVSETUP_FULL

unset -f _pw_run_step
