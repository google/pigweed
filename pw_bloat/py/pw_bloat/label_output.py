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
"""Module for size report ASCII tables from DataSourceMaps."""

import enum
from typing import Iterable, Tuple, Union, Type, List

from pw_bloat.label import DataSourceMap, Optional


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


class _Align(enum.Enum):
    CENTER = 0
    LEFT = 1
    RIGHT = 2


class BloatTableOutput:
    """ASCII Table generator from DataSourceMap."""
    def __init__(self,
                 ds_map: DataSourceMap,
                 col_max_width: int,
                 charset: Union[Type[AsciiCharset],
                                Type[LineCharset]] = AsciiCharset):
        self._data_source_map = ds_map
        self._cs = charset
        self._total_size = 0
        self._col_widths = self._generate_col_width(col_max_width)
        self._col_names = [*self._data_source_map.get_ds_names(), 'sizes']
        self._ascii_table_rows: List[str] = []

    def _generate_col_width(self, col_max_width: int) -> List[int]:
        """Find column width for all data sources and sizes."""
        col_list = [
            len(ds_name) for ds_name in self._data_source_map.get_ds_names()
        ]

        for curr_label in self._data_source_map.labels():
            self._total_size += curr_label.size
            for index, parent_label in enumerate(
                [*curr_label.parents, curr_label.name]):
                if len(parent_label) > col_max_width:
                    col_list[index] = col_max_width
                elif len(parent_label) > col_list[index]:
                    col_list[index] = len(parent_label)

        col_list.append(len(f"{self._total_size:,}"))
        return col_list

    @staticmethod
    def _diff_label_names(
            old_labels: List[Tuple[str, int]],
            new_labels: List[Tuple[str, int]]) -> List[Tuple[str, int]]:
        """Return difference between arrays of labels."""

        if old_labels == []:
            return new_labels

        diff_list: List[Tuple[str, int]] = []
        for (new_lb, old_lb) in zip(new_labels, old_labels):
            if new_lb == old_lb:
                diff_list.append(('', 0))
            else:
                diff_list.append(new_lb)

        return diff_list

    def create_table(self) -> str:
        """Parse DataSourceMap to create ASCII table."""
        curr_lb_hierachy: List[Tuple[str, int]] = []

        self._ascii_table_rows.extend([*self.create_title_row()])

        for curr_label in self._data_source_map.labels():
            new_lb_hierachy = [
                *self.get_ds_label_size(curr_label.parents),
                (curr_label.name, curr_label.size)
            ]
            diff_list = self._diff_label_names(curr_lb_hierachy,
                                               new_lb_hierachy)
            self._ascii_table_rows += self.create_rows_diffs(diff_list)
            curr_lb_hierachy = new_lb_hierachy

        self._ascii_table_rows[-1] = self.row_divider(
            len(diff_list) + 1, self._cs.HH.value)

        self._ascii_table_rows.extend([*self.create_total_row()])

        return '\n'.join(self._ascii_table_rows)

    def get_ds_label_size(
            self, parent_labels: Tuple[str, ...]) -> Iterable[Tuple[str, int]]:
        """Produce label, size pairs from parent label names."""
        parent_label_sizes = []
        for index, target_label in enumerate(parent_labels):
            for curr_label in self._data_source_map.labels(index):
                if curr_label.name == target_label:
                    parent_label_sizes.append(
                        (curr_label.name, curr_label.size))
                    break
        return parent_label_sizes

    def create_total_row(self) -> Iterable[str]:
        complete_total_rows = []
        total_row = ''
        for i in range(len(self._col_names)):
            if i == 0:
                total_row += self.create_cell('Total', False, i, _Align.LEFT)
            elif i == len(self._col_names) - 1:
                total_row += self.create_cell(f"{self._total_size:,}", True, i)
            else:
                total_row += self.create_cell('', False, i, _Align.CENTER)

        complete_total_rows.extend(
            [total_row, self.create_border(False, self._cs.H.value)])
        return complete_total_rows

    def create_rows_diffs(self, diff_list: List[Tuple[str,
                                                      int]]) -> Iterable[str]:
        """Create rows for each label according to its index in diff_list."""
        curr_row = ''
        diff_rows = []
        for index, label_pair in enumerate(diff_list):
            if label_pair[0]:
                for cell_index in range(len(diff_list)):
                    if cell_index == index:
                        if cell_index == 0 and len(self._ascii_table_rows) > 3:
                            diff_rows.append(
                                self.row_divider(len(self._col_names),
                                                 self._cs.H.value))
                        if len(label_pair[0]) > self._col_widths[cell_index]:
                            curr_row = self.multi_row_label(
                                label_pair[0], cell_index)
                            break
                        curr_row += self.create_cell(label_pair[0], False,
                                                     cell_index, _Align.LEFT)
                    else:
                        curr_row += self.create_cell('', False, cell_index)

                #Add size end of current row.
                curr_row += self.create_cell(f"{label_pair[1]:,}", True,
                                             len(self._col_widths) - 1,
                                             _Align.RIGHT)
                diff_rows.append(curr_row)
                curr_row = ''

        return diff_rows

    def create_cell(self,
                    content: str,
                    last_cell: bool,
                    col_index: int,
                    align: Optional[_Align] = _Align.RIGHT) -> str:
        v_border = self._cs.V.value
        pad_diff = self._col_widths[col_index] - len(content)
        padding = (pad_diff // 2) * ' '
        odd_pad = ' ' if pad_diff % 2 == 1 else ''
        string_cell = ''
        if align == _Align.CENTER:
            string_cell = f"{v_border}{odd_pad}{padding}{content}{padding}"
        elif align == _Align.LEFT:
            string_cell = f"{v_border}{content}{padding*2}{odd_pad}"
        elif align == _Align.RIGHT:
            string_cell = f"{v_border}{padding*2}{odd_pad}{content}"

        if last_cell:
            string_cell += self._cs.V.value
        return string_cell

    def multi_row_label(self, content: str, target_col_index: int) -> str:
        """Split content name into multiple rows within correct column."""
        max_len = self._col_widths[target_col_index]
        split_content = '...'.join(
            content[max_len:][i:i + max_len - 3]
            for i in range(0, len(content[max_len:]), max_len - 3))
        split_content = f"{content[:max_len]}...{split_content}"
        split_tab_content = [
            split_content[i:i + max_len]
            for i in range(0, len(split_content), max_len)
        ]
        multi_label = []
        curr_row = ''
        for index, cut_content in enumerate(split_tab_content):
            last_cell = False
            for blank_cell_index in range(len(self._col_names)):
                if blank_cell_index == target_col_index:
                    curr_row += self.create_cell(cut_content, False,
                                                 target_col_index, _Align.LEFT)
                else:
                    if blank_cell_index == len(self._col_names) - 1:
                        if index == len(split_tab_content) - 1:
                            break
                        last_cell = True
                    curr_row += self.create_cell('', last_cell,
                                                 blank_cell_index)
            multi_label.append(curr_row)
            curr_row = ''

        return '\n'.join(multi_label)

    def row_divider(self, col_num: int, h_div: str) -> str:
        l_border = ''
        r_border = ''
        row_div = ''
        for col in range(col_num):
            if col == 0:
                l_border = self._cs.ML.value
                r_border = ''
            elif col == (col_num - 1):
                l_border = self._cs.MM.value
                r_border = self._cs.MR.value
            else:
                l_border = self._cs.MM.value
                r_border = ''

            row_div += f"{l_border}{self._col_widths[col] * h_div}{r_border}"
        return row_div

    def create_title_row(self) -> Iterable[str]:
        title_rows = []
        title_cells = ''
        last_cell = False
        for index, name in enumerate(self._col_names):
            if index == len(self._col_names) - 1:
                last_cell = True
            title_cells += self.create_cell(name, last_cell, index,
                                            _Align.CENTER)
        title_rows.extend([
            self.create_border(True, self._cs.H.value), title_cells,
            self.row_divider(len(self._col_names), self._cs.HH.value)
        ])
        return title_rows

    def create_border(self, top: bool, h_div: str):
        """Top or bottom borders of ASCII table."""
        row_div = ''
        for col in range(len(self._col_names)):
            if top:
                if col == 0:
                    l_div = self._cs.TL.value
                    r_div = ''
                elif col == (len(self._col_names) - 1):
                    l_div = self._cs.TM.value
                    r_div = self._cs.TR.value
                else:
                    l_div = self._cs.TM.value
                    r_div = ''
            else:
                if col == 0:
                    l_div = self._cs.BL.value
                    r_div = ''
                elif col == (len(self._col_names) - 1):
                    l_div = self._cs.BM.value
                    r_div = self._cs.BR.value
                else:
                    l_div = self._cs.BM.value
                    r_div = ''

            row_div += f"{l_div}{self._col_widths[col] * h_div}{r_div}"
        return row_div


class RstOutput(BloatTableOutput):
    """Tabular output in ASCII format, which is also valid RST."""
    def __init__(self, ds_map: DataSourceMap, col_width: int):

        super().__init__(ds_map, col_width, AsciiCharset)
