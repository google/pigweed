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

"""The binary_diff module defines a class which stores size diff information."""

import collections
import csv

from typing import List, Generator, Type

DiffSegment = collections.namedtuple(
    'DiffSegment', ['name', 'before', 'after', 'delta', 'capacity'])
FormattedDiff = collections.namedtuple(
    'FormattedDiff', ['segment', 'before', 'delta', 'after'])


def format_integer(num: int, force_sign: bool = False) -> str:
    """Formats a integer with commas."""
    prefix = '+' if force_sign and num > 0 else ''
    return '{}{:,}'.format(prefix, num)


def format_percent(num: float, force_sign: bool = False) -> str:
    """Formats a decimal ratio as a percentage."""
    prefix = '+' if force_sign and num > 0 else ''
    return '{}{:,.1f}%'.format(prefix, num * 100)


class BinaryDiff:
    """A size diff between two binary files."""

    def __init__(self, label: str):
        self.label = label
        self._segments: collections.OrderedDict = collections.OrderedDict()

    def add_segment(self, segment: DiffSegment):
        """Adds a segment to the diff."""
        self._segments[segment.name] = segment

    def formatted_segments(self) -> Generator[FormattedDiff, None, None]:
        """Yields each of the segments in this diff with formatted data."""

        for segment in self._segments.values():
            if segment.delta == 0:
                continue

            yield FormattedDiff(
                segment.name,
                format_integer(segment.before),
                format_integer(segment.delta, force_sign=True),
                format_integer(segment.after),
            )

    @classmethod
    def from_csv(cls: Type['BinaryDiff'],
                 label: str,
                 raw_csv: List[str]) -> 'BinaryDiff':
        """Parses a BinaryDiff from bloaty's CSV output."""

        diff = cls(label)
        reader = csv.reader(raw_csv)
        for row in reader:
            diff.add_segment(DiffSegment(row[0], int(
                row[5]), int(row[7]), int(row[1]), int(row[3])))

        return diff
