# Copyright 2024 The Pigweed Authors
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

# Only generate completions if $PW_ROOT is set
if test -z "$PW_ROOT"
    exit
end

set status_message_length 0
function print_status_message
    printf '\033[s\033[1B\033[0G'
    set -l status_message "Regenerating completions."
    set status_message_length (math 1 + (string length --visible $status_message))
    printf $status_message
    printf '\033[u'
end

function print_status_indicator
    printf '\033[s\033[1B\033['
    printf $status_message_length
    printf G
    printf '.'
    printf '\033[u'
    set status_message_length (math $status_message_length + 1)
end

print_status_message

complete -c pw --no-files

# pw completion
set --local --append _pw_subcommands (pw --no-banner --tab-complete-format=fish --tab-complete-command "")
print_status_indicator
set --local --append _pw_options (pw --no-banner --tab-complete-format=fish --tab-complete-option "")
print_status_indicator

# pw short and long options
for option in $_pw_options
    set -l complete_args (string split \t -- "$option")

    complete --command pw --condition "__fish_use_subcommand; and not __fish_seen_subcommand_from $_pw_subcommand_names" $complete_args
end

# pw subcommands
for subcommand in $_pw_subcommands
    set --local command_and_help (string split --max 1 ":" "$subcommand")
    set --local command $command_and_help[1]
    set --local help $command_and_help[2]
    set --append _pw_subcommand_names $command

    complete --command pw --condition __fish_use_subcommand --arguments "$command" --description "$help"
end

# pw build completion
if contains build $_pw_subcommand_names
    set --local --append _pw_build_options (pw --no-banner build --tab-complete-format=fish --tab-complete-option "")
    print_status_indicator
    set --local --append _pw_build_recipes (pw --no-banner build --tab-complete-format=fish --tab-complete-recipe "")
    print_status_indicator
    set --local --append _pw_build_presubmit_steps (pw --no-banner build --tab-complete-format=fish --tab-complete-presubmit-step "")
    print_status_indicator

    complete --command pw --condition "__fish_seen_subcommand_from build" -s r -l recipe -x -a "$_pw_build_recipes"
    complete --command pw --condition "__fish_seen_subcommand_from build" -s s -l step -x -a "$_pw_build_presubmit_steps"

    for option in $_pw_build_options
        set -l complete_args (string split \t -- "$option")

        complete --command pw --condition "__fish_seen_subcommand_from build" $complete_args
    end
end
