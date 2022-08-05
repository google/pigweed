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
The label module defines a class to store and manipulate size reports.
"""

from collections import defaultdict
from dataclasses import dataclass
from typing import Iterable, Dict, Sequence, Tuple, List, Generator, Optional

import csv


@dataclass
class Label:
    """Return type of DataSourceMap generator."""
    name: str
    size: int
    parents: Tuple[str, ...] = ()


class _LabelMap:
    """Private module to hold parent and child labels with their size."""
    _label_map: Dict[str, Dict[str, int]]

    def __init__(self):
        self._label_map = defaultdict(lambda: defaultdict(int))

    def remove(self, parent_label: str, child_label: str = None) -> None:
        """Delete entire parent label or the child label."""
        if child_label:
            del self._label_map[parent_label][child_label]
        else:
            del self._label_map[parent_label]

    def diff(self, base: '_LabelMap') -> '_LabelMap':
        """Subtract the current LabelMap to the base."""

    def __getitem__(self, parent_label: str) -> Dict[str, int]:
        """Indexing LabelMap using '[]' operators by specifying a label."""
        return self._label_map[parent_label]

    def map_generator(self) -> Generator:
        for parent_label, label_dict in self._label_map.items():
            yield parent_label, label_dict


class _DataSource:
    """Private module to store a data source name with a _LabelMap."""
    def __init__(self, name: str):
        self._name = name
        self._ds_label_map = _LabelMap()

    def get_name(self) -> str:
        return self._name

    def add_label_size(self, parent_label: str, child_label: str,
                       size: int) -> None:
        self._ds_label_map[parent_label][child_label] += size

    def __getitem__(self, parent_label: str) -> Dict[str, int]:
        return self._ds_label_map[parent_label]

    def label_map_generator(self) -> Generator:
        for parent_label, label_dict in self._ds_label_map.map_generator():
            yield parent_label, label_dict


class DataSourceMap:
    """Module to store an array of DataSources and capacities.

    An organize way to store a hierachy of labels and their sizes.
    Includes a capacity array to hold regex patterns for applying
    capacities to matching label names.

    """
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

    def get_ds_names(self) -> Tuple[str, ...]:
        """List of DataSource names for easy indexing and reference."""
        return tuple(data_source.get_name()
                     for data_source in self._data_sources[1:])

    def labels(self, ds_index: Optional[int] = None) -> Iterable[Label]:
        """Generator that yields a Label depending on specified data source.

        Args:
            ds_index: Integer index of target data source.

        Returns:
            Iterable Label objects.
        """
        ds_index = len(
            self._data_sources) if ds_index is None else ds_index + 2
        yield from self._per_data_source_generator(
            tuple(), self._data_sources[1:ds_index])

    def _per_data_source_generator(
            self, parent_labels: Tuple[str, ...],
            data_sources: Sequence[_DataSource]) -> Iterable[Label]:
        """Recursive generator to return Label based off parent labels."""
        for ds_index, curr_ds in enumerate(data_sources):
            for parent_label, label_map in curr_ds.label_map_generator():
                if not parent_labels:
                    curr_parent = 'total'
                else:
                    curr_parent = parent_labels[-1]
                if parent_label == curr_parent:
                    for child_label, size in label_map.items():
                        if len(data_sources) == 1:
                            yield Label(child_label, size, parent_labels)
                        else:
                            yield from self._per_data_source_generator(
                                (*parent_labels, child_label),
                                data_sources[ds_index + 1:])


def from_bloaty_csv(raw_csv: Iterable[str]) -> DataSourceMap:
    """Read in Bloaty CSV output and store in DataSourceMap."""
    reader = csv.reader(raw_csv)
    top_row = next(reader)
    ds_map_csv = DataSourceMap(top_row[:-2])
    vmsize_index = top_row.index('vmsize')
    for row in reader:
        ds_map_csv.insert_label_hierachy(row[:-2], int(row[vmsize_index]))
    return ds_map_csv
