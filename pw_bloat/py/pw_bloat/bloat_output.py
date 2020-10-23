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
"""Module containing different output formatters for the bloat script."""

import abc
import enum
from typing import (Callable, Collection, Dict, List, Optional, Tuple, Type,
                    Union)

from pw_bloat.binary_diff import BinaryDiff, FormattedDiff


class Output(abc.ABC):
    """An Output produces a size report card in a specific format."""
    def __init__(self,
                 title: Optional[str],
                 diffs: Collection[BinaryDiff] = ()):
        self._title = title
        self._diffs = diffs

    @abc.abstractmethod
    def diff(self) -> str:
        """Creates a report card for a size diff between binaries and a base."""

    @abc.abstractmethod
    def absolute(self) -> str:
        """Creates a report card for the absolute size breakdown of binaries."""


class AsciiCharset(enum.Enum):
    """Set of ASCII characters for drawing tables."""
    TL = '+'
    TM = '+'
    TR = '+'
    ML = '+'
    MM = '+'
    MR = '+'
    BL = '+'
    BM = '+'
    BR = '+'
    V = '|'
    H = '-'
    HH = '='


class LineCharset(enum.Enum):
    """Set of line-drawing characters for tables."""
    TL = '┌'
    TM = '┬'
    TR = '┐'
    ML = '├'
    MM = '┼'
    MR = '┤'
    BL = '└'
    BM = '┴'
    BR = '┘'
    V = '│'
    H = '─'
    HH = '═'


def identity(val: str) -> str:
    """Returns a string unmodified."""
    return val


class TableOutput(Output):
    """Tabular output."""

    LABEL_COLUMN = 'Label'

    def __init__(
            self,
            title: Optional[str],
            diffs: Collection[BinaryDiff] = (),
            charset: Union[Type[AsciiCharset],
                           Type[LineCharset]] = AsciiCharset,
            preprocess: Callable[[str], str] = identity,
            # TODO(frolv): Make this a Literal type.
            justify: str = 'rjust'):
        self._cs = charset
        self._preprocess = preprocess
        self._justify = justify

        super().__init__(title, diffs)

    def diff(self) -> str:
        """Build a tabular diff output showing binary size deltas."""

        # Calculate the width of each column in the table.
        max_label = len(self.LABEL_COLUMN)
        column_widths = [len(field) for field in FormattedDiff._fields]

        for diff in self._diffs:
            max_label = max(max_label, len(diff.label))
            for segment in diff.formatted_segments():
                for i, val in enumerate(segment):
                    val = self._preprocess(val)
                    column_widths[i] = max(column_widths[i], len(val))

        separators = self._row_separators([max_label] + column_widths)

        def title_pad(string: str) -> str:
            padding = (len(separators['top']) - len(string)) // 2
            return ' ' * padding + string

        titles = [
            self._center_align(val.capitalize(), column_widths[i])
            for i, val in enumerate(FormattedDiff._fields)
        ]
        column_names = [self._center_align(self.LABEL_COLUMN, max_label)
                        ] + titles

        rows: List[str] = []

        if self._title is not None:
            rows.extend([
                title_pad(self._title),
                title_pad(self._cs.H.value * len(self._title)),
            ])

        rows.extend([
            separators['top'],
            self._table_row(column_names),
            separators['hdg'],
        ])

        for row, diff in enumerate(self._diffs):
            subrows: List[str] = []

            for segment in diff.formatted_segments():
                subrow: List[str] = []
                label = diff.label if not subrows else ''
                subrow.append(getattr(label, self._justify)(max_label, ' '))
                subrow.extend([
                    getattr(self._preprocess(val),
                            self._justify)(column_widths[i], ' ')
                    for i, val in enumerate(segment)
                ])
                subrows.append(self._table_row(subrow))

            rows.append('\n'.join(subrows))
            rows.append(separators['bot' if row == len(self._diffs) -
                                   1 else 'mid'])

        return '\n'.join(rows)

    def absolute(self) -> str:
        return ''

    def _row_separators(self, column_widths: List[int]) -> Dict[str, str]:
        """Returns row separators for a table based on the character set."""

        # Left, middle, and right characters for each of the separator rows.
        top = (self._cs.TL.value, self._cs.TM.value, self._cs.TR.value)
        mid = (self._cs.ML.value, self._cs.MM.value, self._cs.MR.value)
        bot = (self._cs.BL.value, self._cs.BM.value, self._cs.BR.value)

        def sep(chars: Tuple[str, str, str], heading: bool = False) -> str:
            line = self._cs.HH.value if heading else self._cs.H.value
            lines = [line * width for width in column_widths]
            left = f'{chars[0]}{line}'
            mid = f'{line}{chars[1]}{line}'.join(lines)
            right = f'{line}{chars[2]}'
            return f'{left}{mid}{right}'

        return {
            'top': sep(top),
            'hdg': sep(mid, True),
            'mid': sep(mid),
            'bot': sep(bot),
        }

    def _table_row(self, vals: Collection[str]) -> str:
        """Formats a row of the table with the selected character set."""
        vert = self._cs.V.value
        main = f' {vert} '.join(vals)
        return f'{vert} {main} {vert}'

    @staticmethod
    def _center_align(val: str, width: int) -> str:
        """Left and right pads a value with spaces to center within a width."""
        space = width - len(val)
        padding = ' ' * (space // 2)
        extra = ' ' if space % 2 == 1 else ''
        return f'{extra}{padding}{val}{padding}'


class RstOutput(TableOutput):
    """Tabular output in ASCII format, which is also valid RST."""
    def __init__(self, diffs: Collection[BinaryDiff] = ()):
        # Use RST line blocks within table cells to force each value to appear
        # on a new line in the HTML output.
        def add_rst_block(val: str) -> str:
            return f'| {val}'

        super().__init__(None,
                         diffs,
                         AsciiCharset,
                         preprocess=add_rst_block,
                         justify='ljust')
