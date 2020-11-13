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

_pw_abspath () {
  python -c "import os.path; print(os.path.abspath('$@'))"
}

# Just in case PATH isn't already exported.
export PATH

# Note: Colors are unfortunately duplicated in several places; and removing the
# duplication is not easy. Their locations are:
#
#   - bootstrap.sh
#   - pw_cli/color.py
#   - pw_env_setup/py/pw_env_setup/colors.py
#
# So please keep them matching then modifying them.
pw_none() {
  echo -e "$*"
}

pw_red() {
  echo -e "\033[0;31m$*\033[0m"
}

pw_bold_red() {
  echo -e "\033[1;31m$*\033[0m"
}

pw_yellow() {
  echo -e "\033[0;33m$*\033[0m"
}

pw_bold_yellow() {
  echo -e "\033[1;33m$*\033[0m"
}

pw_green() {
  echo -e "\033[0;32m$*\033[0m"
}

pw_bold_green() {
  echo -e "\033[1;32m$*\033[0m"
}

pw_blue() {
  echo -e "\033[1;34m$*\033[0m"
}

pw_cyan() {
  echo -e "\033[1;36m$*\033[0m"
}

pw_magenta() {
  echo -e "\033[0;35m$*\033[0m"
}

pw_bold_white() {
  echo -e "\033[1;37m$*\033[0m"
}

pw_eval_sourced() {
  if [ "$1" -eq 0 ]; then
    _PW_NAME=$(basename "$PW_SETUP_SCRIPT_PATH" .sh)
    pw_bold_red "Error: Attempting to $_PW_NAME in a subshell"
    pw_red "  Since $_PW_NAME.sh modifies your shell's environment variables,"
    pw_red "  it must be sourced rather than executed. In particular, "
    pw_red "  'bash $_PW_NAME.sh' will not work since the modified "
    pw_red "  environment will get destroyed at the end of the script. "
    pw_red "  Instead, source the script's contents in your shell:"
    pw_red ""
    pw_red "    \$ source $_PW_NAME.sh"
    exit 1
  fi
}

pw_check_root() {
  _PW_ROOT="$1"
  if [[ "$_PW_ROOT" = *" "* ]]; then
    pw_bold_red "Error: The Pigweed path contains spaces\n"
    pw_red "  The path '$_PW_ROOT' contains spaces. "
    pw_red "  Pigweed's Python environment currently requires Pigweed to be "
    pw_red "  at a path without spaces. Please checkout Pigweed in a "
    pw_red "  directory without spaces and retry running bootstrap."
    return
  fi
}

pw_get_env_root() {
  # PW_ENVIRONMENT_ROOT allows developers to specify where the environment
  # should be installed. bootstrap.sh scripts should not use that variable to
  # store the result of this function. This separation allows scripts to assume
  # PW_ENVIRONMENT_ROOT came from the developer and not from a previous
  # bootstrap possibly from another workspace.
  if [ -z "$PW_ENVIRONMENT_ROOT" ]; then
    if [ -n "$PW_PROJECT_ROOT" ]; then
      echo "$PW_PROJECT_ROOT/.environment"
    else
      echo "$PW_ROOT/.environment"
    fi
  else
    echo "$PW_ENVIRONMENT_ROOT"
  fi
}

# Note: This banner is duplicated in three places; which is a lesser evil than
# the contortions that would be needed to share this snippet across shell,
# batch, and Python. Locations:
#
#   - pw_env_setup/util.sh
#   - pw_cli/branding.py
#   - pw_env_setup/py/pw_env_setup/windows_env_start.py
#
_PW_BANNER=$(cat <<EOF
 ▒█████▄   █▓  ▄███▒  ▒█    ▒█ ░▓████▒ ░▓████▒ ▒▓████▄
  ▒█░  █░ ░█▒ ██▒ ▀█▒ ▒█░ █ ▒█  ▒█   ▀  ▒█   ▀  ▒█  ▀█▌
  ▒█▄▄▄█░ ░█▒ █▓░ ▄▄░ ▒█░ █ ▒█  ▒███    ▒███    ░█   █▌
  ▒█▀     ░█░ ▓█   █▓ ░█░ █ ▒█  ▒█   ▄  ▒█   ▄  ░█  ▄█▌
  ▒█      ░█░ ░▓███▀   ▒█▓▀▓█░ ░▓████▒ ░▓████▒ ▒▓████▀
EOF
)

_pw_banner() {
  if [ -z "$PW_ENVSETUP_QUIET" ] && [ -z "$PW_ENVSETUP_NO_BANNER" ]; then
    pw_magenta "$_PW_BANNER\n"
  fi
}

_PW_BANNER_FUNC="_pw_banner"

_pw_hello() {
  _PW_TEXT="$1"
  if [ -n "$PW_BANNER_FUNC" ]; then
    _PW_BANNER_FUNC="$PW_BANNER_FUNC"
  fi
  if [ -z "$PW_ENVSETUP_QUIET" ]; then
    pw_green "\n  WELCOME TO...\n"
    "$_PW_BANNER_FUNC"
    pw_green "$_PW_TEXT"
  fi
}

pw_deactivate() {
  # Assume PW_ROOT and PW_PROJECT_ROOT has already been set and we need to
  # preserve their values.
  _NEW_PW_ROOT="$PW_ROOT"
  _NEW_PW_PROJECT_ROOT="$PW_PROJECT_ROOT"

  # Find deactivate script and run it.
  _PW_DEACTIVATE_SH="$_PW_ACTUAL_ENVIRONMENT_ROOT/deactivate.sh"
  if [ -f "$_PW_DEACTIVATE_SH" ]; then
    . "$_PW_DEACTIVATE_SH"
  fi

  # If there's a _pw_deactivate function run it. Redirect output to /dev/null
  # in case _pw_deactivate doesn't exist.
  if [ -n "$(command -v _pw_deactivate)" ]; then
    _pw_deactivate &> /dev/null
  fi

  # Restore.
  PW_ROOT="$_NEW_PW_ROOT"
  export PW_ROOT
  PW_PROJECT_ROOT="$_NEW_PW_PROJECT_ROOT"
  export PW_PROJECT_ROOT
}

# The next three functions use the following variables.
# * PW_BANNER_FUNC: function to print banner
# * PW_BOOTSTRAP_PYTHON: specific Python interpreter to use for bootstrap
# * PW_USE_GCS_ENVSETUP: attempt to grab env setup executable from GCS if "true"
# * PW_ROOT: path to Pigweed root
# * PW_ENVSETUP_QUIET: limit output if "true"
#
# All arguments passed in are passed on to env_setup.py in pw_bootstrap,
# pw_activate takes no arguments, and pw_finalize takes the name of the script
# "bootstrap" or "activate" and the path to the setup script written by
# bootstrap.sh.
pw_bootstrap() {
  _pw_hello "  BOOTSTRAP! Bootstrap may take a few minutes; please be patient.\n"

  # Allow forcing a specific version of Python for testing pursposes.
  if [ -n "$PW_BOOTSTRAP_PYTHON" ]; then
    _PW_PYTHON="$PW_BOOTSTRAP_PYTHON"
  elif which python &> /dev/null; then
    _PW_PYTHON=python
  else
    pw_bold_red "Error: No system Python present\n"
    pw_red "  Pigweed's bootstrap process requires a local system Python."
    pw_red "  Please install Python on your system, add it to your PATH"
    pw_red "  and re-try running bootstrap."
    return
  fi

  if [ -n "$PW_USE_GCS_ENVSETUP" ]; then
    _PW_ENV_SETUP="$("$PW_ROOT/pw_env_setup/get_pw_env_setup.sh")"
  fi

  if [ -n "$_PW_ENV_SETUP" ]; then
    "$_PW_ENV_SETUP" "$@"
  else
    "$_PW_PYTHON" "$PW_ROOT/pw_env_setup/py/pw_env_setup/env_setup.py" "$@"
  fi
}

pw_activate() {
  _pw_hello "  ACTIVATOR! This sets your shell environment variables.\n"
}

pw_finalize() {
  _PW_NAME="$1"
  _PW_SETUP_SH="$2"
  if [ -f "$_PW_SETUP_SH" ]; then
    . "$_PW_SETUP_SH"

    if [ "$?" -eq 0 ]; then
      if [ "$_PW_NAME" = "bootstrap" ] && [ -z "$PW_ENVSETUP_QUIET" ]; then
        echo "To activate this environment in the future, run this in your "
        echo "terminal:"
        echo
        pw_green "  source ./activate.sh\n"
      fi
    else
      pw_red "Error during $_PW_NAME--see messages above."
    fi
  else
    pw_red "Error during $_PW_NAME--see messages above."
  fi
}

pw_cleanup() {
  unset _PW_BANNER
  unset _PW_BANNER_FUNC
  unset _PW_ENV_SETUP
  unset _PW_NAME
  unset _PW_PYTHON
  unset _PW_SETUP_SH
  unset _PW_DEACTIVATE_SH
  unset _NEW_PW_ROOT

  unset _pw_abspath
  unset pw_none
  unset pw_red
  unset pw_bold_red
  unset pw_yellow
  unset pw_bold_yellow
  unset pw_green
  unset pw_bold_green
  unset pw_blue
  unset pw_cyan
  unset pw_magenta
  unset pw_bold_white
  unset pw_eval_sourced
  unset pw_check_root
  unset pw_get_env_root
  unset _pw_banner
  unset pw_bootstrap
  unset pw_activate
  unset pw_finalize
  unset _pw_cleanup
}
