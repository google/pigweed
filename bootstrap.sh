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

# This script must be tested on bash, zsh, and dash.

_pw_abspath () {
  python -c "import os.path; print(os.path.abspath('$@'))"
}

_pw_red() {
  echo -e "\033[0;31m$*\033[0m"
}

_pw_bold_red() {
  echo -e "\033[1;31m$*\033[0m"
}

_pw_green() {
  echo -e "\033[0;32m$*\033[0m"
}

_pw_bright_magenta() {
  echo -e "\033[0;35m$*\033[0m"
}

_PIGWEED_BANNER=$(cat <<EOF
 ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
  ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
  ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
  ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
  ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀
EOF
)

# Users are not expected to set PW_CHECKOUT_ROOT, it's only used because it
# seems to be impossible to reliably determine the path to a sourced file in
# dash when sourced from a dash script instead of a dash interactive prompt.
# To reinforce that users should not be using PW_CHECKOUT_ROOT, it is cleared
# here after it is used, and other pw tools will complain if they see that
# variable set.
# TODO(mohrr) find out a way to do this without PW_CHECKOUT_ROOT.
if test -n "$PW_CHECKOUT_ROOT"; then
  PW_SETUP_SCRIPT_PATH=$(_pw_abspath "$PW_CHECKOUT_ROOT/bootstrap.sh")
  unset PW_CHECKOUT_ROOT
# Shell: bash.
elif test -n "$BASH"; then
  PW_SETUP_SCRIPT_PATH=$(_pw_abspath $BASH_SOURCE)
# Shell: zsh.
elif test -n "$ZSH_NAME"; then
  PW_SETUP_SCRIPT_PATH=$(_pw_abspath ${(%):-%N})
# Shell: dash.
elif test ${0##*/} = dash; then
  PW_SETUP_SCRIPT_PATH=$(_pw_abspath \
    $(lsof -p $$ -Fn0 | tail -1 | sed 's#^[^/]*##;'))
# If everything else fails, try $0. It could work.
else
  PW_SETUP_SCRIPT_PATH=$(_pw_abspath $0)
fi

PW_ROOT=$(dirname $PW_SETUP_SCRIPT_PATH)
export PW_ROOT

SETUP_SH="$PW_ROOT/pw_env_setup/.env_setup.sh"

if [ -z "$PW_ENVSETUP_QUIET" ]; then
  _pw_green "\n  WELCOME TO...\n"
  _pw_bright_magenta "$_PIGWEED_BANNER\n"
fi

# Run full bootstrap when invoked as bootstrap, or env file is missing/empty.
[ $(basename $PW_SETUP_SCRIPT_PATH) = "bootstrap.sh" ] || \
  [ ! -f $SETUP_SH ] || \
  [ ! -s $SETUP_SH ]
_PW_IS_BOOTSTRAP=$?

if [ $_PW_IS_BOOTSTRAP -eq 0 ]; then
  _PW_NAME="bootstrap"

  if [ -z "$PW_ENVSETUP_QUIET" ]; then
    _pw_green "  BOOTSTRAP! Bootstrap may take a few minutes; please be patient.\n"
  fi

  # Allow forcing a specific version of Python for testing pursposes.
  if [ -n "$PW_BOOTSTRAP_PYTHON" ]; then
    PYTHON="$PW_BOOTSTRAP_PYTHON"
  elif which python &> /dev/null; then
    PYTHON=python
  else
    _pw_bold_red "Error: No system Python present\n"
    _pw_red "  Pigweed's bootstrap process requires a local system Python."
    _pw_red "  Please install Python on your system, add it to your PATH"
    _pw_red "  and re-try running bootstrap."
    return
  fi

  $PYTHON $PW_ROOT/pw_env_setup/py/pw_env_setup/env_setup.py --shell-file $SETUP_SH
else
  _PW_NAME="activate"

  if [ -z "$PW_ENVSETUP_QUIET" ]; then
    _pw_green "  ACTIVATOR! This sets your shell environment variables.\n"
  fi
fi

if [ -f $SETUP_SH ]; then
  . $SETUP_SH

  if [ $? == 0 ]; then
    if [ $_PW_IS_BOOTSTRAP -eq 0 ] && [ -z "$PW_ENVSETUP_QUIET" ]; then
      echo "To activate this environment in the future, run this in your "
      echo "terminal:"
      echo
      _pw_green "  . ./activate.sh\n"
    fi
  else
    _pw_red "Error during $_PW_NAME--see messages above."
  fi
else
  _pw_red "Error during $_PW_NAME--see messages above."
fi

unset _PW_IS_BOOTSTRAP
unset _PW_NAME
unset _PIGWEED_BANNER
unset _pw_abspath
unset _pw_red
unset _pw_bold_red
unset _pw_green
unset _pw_bright_magenta
