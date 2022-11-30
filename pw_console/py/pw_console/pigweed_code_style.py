# Copyright 2021 The Pigweed Authors
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
"""Brighter PigweedCode pygments style."""

import copy
import re

from prompt_toolkit.styles.style_transformation import get_opposite_color

from pygments.style import Style  # type: ignore
from pygments.token import Comment, Keyword, Name, Generic  # type: ignore
from pygments_style_dracula.dracula import DraculaStyle  # type: ignore

_style_list = copy.copy(DraculaStyle.styles)

# Darker Prompt
_style_list[Generic.Prompt] = '#bfbfbf'
# Lighter comments
_style_list[Comment] = '#778899'
_style_list[Comment.Hashbang] = '#778899'
_style_list[Comment.Multiline] = '#778899'
_style_list[Comment.Preproc] = '#ff79c6'
_style_list[Comment.Single] = '#778899'
_style_list[Comment.Special] = '#778899'
# Lighter output
_style_list[Generic.Output] = '#f8f8f2'
_style_list[Generic.Emph] = '#f8f8f2'
# Remove 'italic' from these
_style_list[Keyword.Declaration] = '#8be9fd'
_style_list[Name.Builtin] = '#8be9fd'
_style_list[Name.Label] = '#8be9fd'
_style_list[Name.Variable] = '#8be9fd'
_style_list[Name.Variable.Class] = '#8be9fd'
_style_list[Name.Variable.Global] = '#8be9fd'
_style_list[Name.Variable.Instance] = '#8be9fd'

_COLOR_REGEX = re.compile(r'#(?P<hex>[0-9a-fA-F]{6}) *(?P<rest>.*?)$')


def swap_light_dark(color: str) -> str:
    if not color:
        return color
    match = _COLOR_REGEX.search(color)
    if not match:
        return color
    match_groups = match.groupdict()
    opposite_color = get_opposite_color(match_groups['hex'])
    if not opposite_color:
        return color
    rest = match_groups.get('rest', '')
    return '#' + ' '.join([opposite_color, rest])


class PigweedCodeStyle(Style):

    background_color = '#2e2e2e'
    default_style = ''

    styles = _style_list


class PigweedCodeLightStyle(Style):

    background_color = '#f8f8f8'
    default_style = ''

    styles = {key: swap_light_dark(value) for key, value in _style_list.items()}
