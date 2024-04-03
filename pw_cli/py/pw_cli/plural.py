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
"""Utilities for handling singular/plural forms in logs and tooling."""


def plural(
    items_or_count,
    singular: str,
    count_format='',
    these: bool = False,
    number: bool = True,
    are: bool = False,
    exist: bool = False,
) -> str:
    """Returns the singular or plural form of a word based on a count.

    Args:
        items_or_count: Number of items or a collection of items
        singular: Singular form of the name of the item
        count_format: .format()-style specification for items_or_count
        these: Prefix the string with "this" or "these", depending on number
        number: Include the number in the return string (e.g., "3 things" vs.
            "things")
        are: Suffix the string with "is" or "are", depending on number
        exist: Suffix the string with "exists" or "exist", depending on number
    """

    try:
        count = len(items_or_count)
    except TypeError:
        count = items_or_count

    prefix = ('this ' if count == 1 else 'these ') if these else ''
    num = f'{count:{count_format}} ' if number else ''

    suffix = ''
    if are and exist:
        raise ValueError(f'cannot combine are ({are}) and exist ({exist})')
    if are:
        suffix = ' is' if count == 1 else ' are'
    if exist:
        suffix = ' exists' if count == 1 else ' exist'

    if singular.endswith('y'):
        result = f'{singular[:-1]}{"y" if count == 1 else "ies"}'
    elif singular.endswith('s'):
        result = f'{singular}{"" if count == 1 else "es"}'
    else:
        result = f'{singular}{"" if count == 1 else "s"}'

    return f'{prefix}{num}{result}{suffix}'
