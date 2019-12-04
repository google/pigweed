# Copyright 2019 The Pigweed Authors
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


def _make_color(*codes):
    # Apply all the requested ANSI color codes. Note that this is unbalanced
    # with respect to the reset, which only requires a '0' to erase all codes.
    start = ''.join(f'\033[{code}m' for code in codes)
    reset = '\033[0m'

    return lambda msg: f'{start}{msg}{reset}'


# TODO(keir): Totally replace this Color object with something more complete
# like the 'colorful' module.
class Color:
    """Helpers to surround text with ASCII color escapes"""
    red = _make_color(31, 1)
    bold_red = _make_color(30, 41)
    yellow = _make_color(33, 1)
    bold_yellow = _make_color(30, 43, 1)
    green = _make_color(32)
    bold_green = _make_color(30, 42)
    blue = _make_color(34, 1)
    cyan = _make_color(36, 1)
    magenta = _make_color(35, 1)
    bold_white = _make_color(37, 1)
    black_on_white = _make_color(30, 47)  # black fg white bg
