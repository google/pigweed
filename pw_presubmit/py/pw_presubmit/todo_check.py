# Copyright 2022 The Pigweed Authors
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
"""Check the formatting of TODOs."""

import re
from typing import Sequence

from pw_presubmit import PresubmitContext, filter_paths

EXCLUDE: Sequence[str] = (
    # Metadata
    r'^docker/tag$',
    r'\byarn.lock$',
    # Data files
    r'\.bin$',
    r'\.csv$',
    r'\.elf$',
    r'\.gif$',
    r'\.jpg$',
    r'\.json$',
    r'\.png$',
    r'\.svg$',
    r'\.xml$',
)

# todo-check: disable
BUGS_ONLY = re.compile(r'\bTODO\(b/\d+\).*\w')
BUGS_OR_USERNAMES = re.compile(r'\bTODO\((?:b/\d+|[a-z]+)\).*\w')
_TODO = re.compile(r'\bTODO\b')
# todo-check: enable

# If seen, ignore this line and the next.
_IGNORE = 'todo-check: ignore'

# Ignore a whole section. Please do not change the order of these lines.
_DISABLE = 'todo-check: disable'
_ENABLE = 'todo-check: enable'


def create(todo_pattern: re.Pattern = BUGS_ONLY,
           exclude: Sequence[str] = EXCLUDE):
    """Create a todo_check presubmit step that uses the given pattern."""
    @filter_paths(exclude=exclude)
    def todo_check(ctx: PresubmitContext):
        """Presubmit check that makes sure todo lines match the pattern."""
        for path in ctx.paths:
            with open(path) as file:
                enabled = True
                prev = ''

                for i, line in enumerate(file, 1):
                    if _DISABLE in line:
                        enabled = False
                    elif _ENABLE in line:
                        enabled = True

                    if enabled:
                        if _IGNORE not in line and _IGNORE not in prev:
                            if _TODO.search(line):
                                if not todo_pattern.search(line):
                                    # todo-check: ignore
                                    ctx.fail(f'Bad TODO on line {i}:', path)
                                    ctx.fail(f'    {line.strip()}')

                    prev = line

    return todo_check
