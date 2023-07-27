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

if [[ -n ${ZSH_VERSION-} ]]; then
  autoload -U +X bashcompinit && bashcompinit
fi

__pwcomp_words_include ()
{
  local i=1
  while [[ $i -lt $COMP_CWORD ]]; do
    if [[ "${COMP_WORDS[i]}" = "$1" ]]; then
      return 0
    fi
    i=$((++i))
  done
  return 1
}

# Find the previous non-switch word
__pwcomp_prev ()
{
  local idx=$((COMP_CWORD - 1))
  local prv="${COMP_WORDS[idx]}"
  while [[ $prv == -* ]]; do
    idx=$((--idx))
    prv="${COMP_WORDS[idx]}"
  done
}


__pwcomp ()
{
  # break $1 on space, tab, and newline characters,
  # and turn it into a newline separated list of words
  local list s sep=$'\n' IFS=$' '$'\t'$'\n'
  local cur="${COMP_WORDS[COMP_CWORD]}"

  for s in $1; do
    __pwcomp_words_include "$s" && continue
    list="$list$s$sep"
  done

  case "$cur" in
  --*=)
    COMPREPLY=()
    ;;
  *)
    IFS=$sep
    COMPREPLY=( $(compgen -W "$list" -- "$cur" | sed -e 's/[^=]$/& /g') )
    ;;
  esac
}

