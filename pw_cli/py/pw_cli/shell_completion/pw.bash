# Copyright 2023 The Pigweed Authors
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

# Source common bash completion functions
# Path to this directory, works in bash and zsh
COMPLETION_DIR=$(dirname "${BASH_SOURCE[0]-$0}")
. "${COMPLETION_DIR}/common.bash"

_pw () {
  local i=1 cmd

  if [[ -n ${ZSH_VERSION-} ]]; then
    emulate -L bash
    setopt KSH_TYPESET

    # Workaround zsh's bug that leaves 'words' as a special
    # variable in versions < 4.3.12
    typeset -h words
  fi

  # find the subcommand
  while [[ $i -lt $COMP_CWORD ]]; do
    local s="${COMP_WORDS[i]}"
    case "$s" in
    --*) ;;
    -*) ;;
    *)  cmd="$s"
      break
      ;;
    esac
    i=$((++i))
  done

  if [[ $i -eq $COMP_CWORD ]]; then
    local cur="${COMP_WORDS[COMP_CWORD]}"
    case "$cur" in
      -*)
        local all_options=$(pw --no-banner --tab-complete-option "")
        __pwcomp "${all_options}"
        return
        ;;
      *)
        local all_commands=$(pw --no-banner --tab-complete-command "")
        __pwcomp "${all_commands}"
        return
        ;;
    esac
    return
  fi

  # subcommands have their own completion functions
  case "$cmd" in
    help)
      local all_commands=$(pw --no-banner --tab-complete-command "")
      __pwcomp "${all_commands}"
      ;;
    *)
      # If the command is 'build' and a function named _pw_build exists, then run it.
      [[ $(type -t _pw_$cmd) == function ]] && _pw_$cmd
      ;;
  esac
}

complete -o bashdefault -o default -o nospace -F _pw pw
