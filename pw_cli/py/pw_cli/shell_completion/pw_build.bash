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

__pw_complete_recipes () {
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local all_recipes=$(pw --no-banner build --tab-complete-recipe "${cur}")
  COMPREPLY=($(compgen -W "$all_recipes" -- "$cur"))
}

__pw_complete_presubmit_steps () {
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local all_recipes=$(pw --no-banner build --tab-complete-presubmit-step "${cur}")
  COMPREPLY=($(compgen -W "$all_recipes" -- "$cur"))
}

_pw_build () {
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local prev="${COMP_WORDS[COMP_CWORD-1]}"

  case "$prev" in
  -r|--recipe)
    __pw_complete_recipes
    return
    ;;
  -s|--step)
    __pw_complete_presubmit_steps
    return
    ;;
  --logfile)
    # Complete a file
    COMPREPLY=($(compgen -f "$cur"))
    return
    ;;
  *)
    ;;
  esac

  case "$cur" in
  -*)
    __pwcomp
    local all_options=$(pw --no-banner build --tab-complete-option "")
    __pwcomp "${all_options}"
    return
    ;;
  esac
  # Non-option args go here
  COMPREPLY=()
}
