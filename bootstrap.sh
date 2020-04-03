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
  PW_SETUP_SCRIPT_PATH="$(_pw_abspath "$PW_CHECKOUT_ROOT/bootstrap.sh")"
  unset PW_CHECKOUT_ROOT
# Shell: bash.
elif test -n "$BASH"; then
  PW_SETUP_SCRIPT_PATH="$(_pw_abspath "$BASH_SOURCE")"
# Shell: zsh.
elif test -n "$ZSH_NAME"; then
  PW_SETUP_SCRIPT_PATH="$(_pw_abspath "${(%):-%N}")"
# Shell: dash.
elif test ${0##*/} = dash; then
  PW_SETUP_SCRIPT_PATH="$(_pw_abspath \
    "$(lsof -p $$ -Fn0 | tail -1 | sed 's#^[^/]*##;')")"
# If everything else fails, try $0. It could work.
else
  PW_SETUP_SCRIPT_PATH="$(_pw_abspath "$0")"
fi

# Check if this file is being executed or sourced.
_pw_sourced=0
if [ -n "$SWARMING_BOT_ID" ]; then
  # If set we're running on swarming and don't need this check.
  _pw_sourced=1
elif [ -n "$ZSH_EVAL_CONTEXT" ]; then
  case $ZSH_EVAL_CONTEXT in *:file) _pw_sourced=1;; esac
elif [ -n "$KSH_VERSION" ]; then
  [ "$(cd $(dirname -- $0) && pwd -P)/$(basename -- $0)" != \
    "$(cd $(dirname -- ${.sh.file}) && pwd -P)/$(basename -- ${.sh.file})" ] \
    && _pw_sourced=1
elif [ -n "$BASH_VERSION" ]; then
  (return 0 2>/dev/null) && _pw_sourced=1
else  # All other shells: examine $0 for known shell binary filenames
  # Detects `sh` and `dash`; add additional shell filenames as needed.
  case ${0##*/} in sh|dash) _pw_sourced=1;; esac
fi

if [ "$_pw_sourced" -eq 0 ]; then
  _PW_NAME=$(basename "$PW_SETUP_SCRIPT_PATH" .sh)
  _pw_bold_red "Error: Attempting to $_PW_NAME in a subshell"
  _pw_red "  Since $_PW_NAME.sh modifies your shell's environment variables, it"
  _pw_red "  must be sourced rather than executed. In particular, "
  _pw_red "  'bash $_PW_NAME.sh' will not work since the modified environment "
  _pw_red "  will get destroyed at the end of the script. Instead, source the "
  _pw_red "  script's contents in your shell:"
  _pw_red ""
  _pw_red "    \$ source $_PW_NAME.sh"
  exit 1
fi

PW_ROOT="$(dirname "$PW_SETUP_SCRIPT_PATH")"

if [[ "$PW_ROOT" = *" "* ]]; then
  _pw_bold_red "Error: The Pigweed path contains spaces\n"
  _pw_red "  The path '$PW_ROOT' contains spaces. "
  _pw_red "  Pigweed's Python environment currently requires Pigweed to be "
  _pw_red "  at a path without spaces. Please checkout Pigweed in a directory "
  _pw_red "  without spaces and retry running bootstrap."
  return
fi

export PW_ROOT

SETUP_SH="$PW_ROOT/pw_env_setup/.env_setup.sh"

if [ -z "$PW_ENVSETUP_QUIET" ]; then
  _pw_green "\n  WELCOME TO...\n"
  _pw_bright_magenta "$_PIGWEED_BANNER\n"
fi

# Run full bootstrap when invoked as bootstrap, or env file is missing/empty.
[ "$(basename "$PW_SETUP_SCRIPT_PATH")" = "bootstrap.sh" ] || \
  [ ! -f "$SETUP_SH" ] || \
  [ ! -s "$SETUP_SH" ]
_PW_IS_BOOTSTRAP="$?"

if [ "$_PW_IS_BOOTSTRAP" -eq 0 ]; then
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

  _PW_OLD_CIPD_PACKAGE_FILES="$PW_CIPD_PACKAGE_FILES"
  PW_CIPD_PACKAGE_FILES="$PW_ROOT/pw_env_setup/py/pw_env_setup/cipd_setup/*.json:$PW_CIPD_PACKAGE_FILES"
  export PW_CIPD_PACKAGE_FILES

  _PW_OLD_VIRTUALENV_REQUIREMENTS="$PW_VIRTUALENV_REQUIREMENTS"
  PW_VIRTUALENV_REQUIREMENTS="$PW_ROOT/pw_env_setup/py/pw_env_setup/virtualenv_setup/requirements.txt:$PW_VIRTUALENV_REQUIREMENTS"
  export PW_VIRTUALENV_REQUIREMENTS

  _PW_OLD_VIRTUALENV_SETUP_PY_ROOTS="$PW_VIRTUALENV_SETUP_PY_ROOTS"
  PW_VIRTUALENV_SETUP_PY_ROOTS="$PW_ROOT/*:$PW_VIRTUALENV_SETUP_PY_ROOTS"
  export PW_VIRTUALENV_SETUP_PY_ROOTS

  _PW_OLD_CARGO_PACKAGE_FILES="$PW_CARGO_PACKAGE_FILES"
  PW_CARGO_PACKAGE_FILES="$PW_ROOT/pw_env_setup/py/pw_env_setup/cargo_setup/packages.txt:$PW_CARGO_PACKAGE_FILES"
  export PW_CARGO_PACKAGE_FILES

  "$PYTHON" "$PW_ROOT/pw_env_setup/py/pw_env_setup/env_setup.py" --shell-file "$SETUP_SH"


  PW_CIPD_PACKAGE_FILES="$_PW_OLD_CIPD_PACKAGE_FILES"
  PW_VIRTUALENV_REQUIREMENTS="$_PW_OLD_VIRTUALENV_REQUIREMENTS"
  PW_VIRTUALENV_SETUP_PY_ROOTS="$_PW_OLD_VIRTUALENV_SETUP_PY_ROOTS"
  PW_CARGO_PACKAGE_FILES="$_PW_OLD_CARGO_PACKAGE_FILES"
else
  _PW_NAME="activate"

  if [ -z "$PW_ENVSETUP_QUIET" ]; then
    _pw_green "  ACTIVATOR! This sets your shell environment variables.\n"
  fi
fi

if [ -f "$SETUP_SH" ]; then
  . "$SETUP_SH"

  if [ "$?" -eq 0 ]; then
    if [ "$_PW_IS_BOOTSTRAP" -eq 0 ] && [ -z "$PW_ENVSETUP_QUIET" ]; then
      echo "To activate this environment in the future, run this in your "
      echo "terminal:"
      echo
      _pw_green "  source ./activate.sh\n"
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
unset _PW_OLD_CIPD_PACKAGE_FILES
unset _PW_OLD_VIRTUALENV_REQUIREMENTS
unset _PW_OLD_VIRTUALENV_SETUP_PY_ROOTS
unset _PW_OLD_CARGO_PACKAGE_FILES
unset _pw_abspath
unset _pw_red
unset _pw_bold_red
unset _pw_green
unset _pw_bright_magenta
unset _pw_sourced
