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
"""
LabelMap and Label moduled defines the data structure to hold
size reports from Bloaty.
"""

from collections import defaultdict
from typing import Iterable, Dict, Tuple, List

import csv


class _LabelMap:
    """Private module to store a parent label and all of its
    child labels with its corresponding size in a nested dictionary."""
    _label_map: Dict[str, Dict[str, int]]

    def __init__(self):
        self._label_map = defaultdict(lambda: defaultdict(int))

    def remove(self, parent_label: str, label: str = None) -> None:
        """Delete entire parent label or the child label."""
        if label:
            del self._label_map[parent_label][label]
        else:
            del self._label_map[parent_label]

    def diff(self, base: '_LabelMap') -> '_LabelMap':
        """Subtract the current LabelMap to the base."""

    def __getitem__(self, parent_label: str) -> Dict[str, int]:
        """Allow indexing of a LabelMap using '[]' operators
        by specifying a label to access."""
        return self._label_map[parent_label]


class _DataSource:
    """Private module to store a data source name with a _LabelMap."""
    def __init__(self, name: str):
        self._name = name
        self._ds_label_map = _LabelMap()

    def get_name(self) -> str:
        return self._name

    def add_label_size(self, parent_label: str, label: str, size: int) -> None:
        self._ds_label_map[parent_label][label] += size

    def __getitem__(self, parent_label: str) -> Dict[str, int]:
        return self._ds_label_map[parent_label]


class DataSourceMap:
    """Module with an array of DataSources to organize a hierachy
    of labels and their sizes. Includes a capacity array to hold regex
    patterns for applying capacities to matching labels."""
    def __init__(self, data_sources_names: Iterable[str]):
        self._data_sources = list(
            _DataSource(name) for name in ['base', *data_sources_names])
        self._capacity_array: List[Tuple[str, int]] = []

    def insert_label_hierachy(self, label_hierarchy: Iterable[str],
                              size: int) -> None:
        """Insert a hierachy of labels with its size."""

        # Insert initial '__base__' data source that holds the
        # running total size.
        self._data_sources[0].add_label_size('__base__', 'total', size)
        complete_label_hierachy = ['total', *label_hierarchy]
        for index in range(len(complete_label_hierachy) - 1):
            if complete_label_hierachy[index]:
                self._data_sources[index + 1].add_label_size(
                    complete_label_hierachy[index],
                    complete_label_hierachy[index + 1], size)

    def add_capacity(self, regex_pattern: str, capacity: int) -> None:
        """Insert regex pattern and capacity into dictionary."""
        self._capacity_array.append((regex_pattern, capacity))

    def get_total_size(self) -> int:
        return self._data_sources[0]['__base__']['total']


def from_bloaty_csv(raw_csv: Iterable[str]) -> DataSourceMap:
    """Read in Bloaty CSV output and store in DataSourceMap."""
    reader = csv.reader(raw_csv)
    top_row = next(reader)
    ds_map = DataSourceMap(top_row[:-2])
    for row in reader:
        ds_map.insert_label_hierachy(row[:-2], int(row[-2]))
    return ds_map
