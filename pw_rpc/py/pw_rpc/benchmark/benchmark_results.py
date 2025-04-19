# Copyright 2025 The Pigweed Authors
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
"""This module provides classes for measuring RPC benchmark tests."""

from abc import ABC, abstractmethod
from collections import Counter
from collections.abc import Iterable
from dataclasses import dataclass, field
from statistics import mean, median, quantiles, stdev
from typing import TextIO

from pw_status import Status


@dataclass(kw_only=True)
class DataStatistics:
    """Dynamically calculates various statistics on a stored data set.

    This class is primarily meant to aggregate data samples from RPC tests
    and provide calculated statistics when its accessor functions are
    invoked.  For example, a test could record all the RPC roundtrip times
    and then call ``average`` to return the average roundtrip time.

    This class is usually instantiated (automatically) in a parent object
    that represents the test output and parameters for a given test.

    Use the ``store`` function to add data to the dataset.
    """

    datapoints: list[float] = field(default_factory=list)

    def as_dict(self, quantile_divisions: int) -> dict:
        return dict(
            average=self.average(),
            max=self.max(),
            median=self.median(),
            min=self.min(),
            quantiles=self.quantiles(divisions=quantile_divisions),
            size=self.size(),
            std_dev=self.std_dev(),
            sum=self.sum(),
        )

    def average(self) -> float:
        return mean(self.datapoints)

    def max(self) -> float:
        return max(self.datapoints)

    def median(self) -> float:
        return median(self.datapoints)

    def min(self) -> float:
        return min(self.datapoints)

    def quantiles(self, divisions) -> list:
        if self.size() > 1:
            return quantiles(
                self.datapoints,
                n=min(self.size(), divisions),
                method='exclusive',
            )
        return self.datapoints

    def size(self) -> int:
        return len(self.datapoints)

    def std_dev(self) -> float:
        if self.size() > 1:
            return stdev(self.datapoints)
        return 0.0

    def sum(self) -> float:
        return sum(self.datapoints)

    def store(self, data: float):
        self.datapoints.append(data)


# TODO: https://pwbug.dev/380584579 - convert this class into a factory when
# more tests are added.  Maybe in a separate file.
class BaseResult(ABC):
    """Provides a base class for easy formatting of different test results.

    The result class is intended to represent the RPC response of a single
    RPC and provide any relevant calculations/helper functions for that data.
    Users can inherit this class into their own, customized result classes
    when creating custom tests.
    """

    status: Status = Status.OK

    def print_results(self, file: TextIO | None = None) -> None:
        for line in self.results():
            print(line, file=file)

    @abstractmethod
    def results(self) -> Iterable[str]:
        pass


@dataclass(kw_only=True, frozen=True)
class FindMaxEchoBufferSizeResult(BaseResult):
    """Encapsulates the data of the find_max_echo_buffer_size function.

    This class provides helper and accessor functions to calculate the results
    of a max echo buffer size test in the ``benchmark.py`` file.  Use the
    ``print_results`` function to show the calculated test metrics (typically
    only done at the end of a test).
    """

    last_error_message: str = ""
    limited_by_max_size: bool = False
    max_packet_size_bytes: int = 0
    max_payload_size_bytes: int = 0
    payload_start_size_bytes: int = 0

    def results(self) -> Iterable[str]:
        """Returns a printable iterable of test results.

        Args:
            None.

        Returns:
            Iterable: An iterable string useful for printing.
        """
        yield "find_max_echo_buffer_size test results:"
        yield f"Starting packet size:     {self.payload_start_size_bytes} B"
        yield f"Max measured packet size: {self.max_payload_size_bytes} B"
        if 0 > self.max_packet_size_bytes:
            yield "Max allowed packet size:  Unbounded."
        else:
            yield f"Max allowed packet size:  {self.max_packet_size_bytes} B"
        yield f"Last error message:       {self.last_error_message}"
        # pylint: disable-next=line-too-long
        yield f"Test stopped by benchmark option's max packet size? {self.limited_by_max_size}"


@dataclass(kw_only=True)
class GoodputStatisticsResult(BaseResult):
    """Encapsulates the data of the measure_rpc_goodput_statistics function.

    This class provides helper and accessor functions to calculate the results
    of a goodput test in the ``benchmark.py`` file.  Use the ``update``
    function to add new data to an instantiated object, and ``print_results``
    to show the calculated test metrics (typically only done at the end of a
    test).

    This class can be used for tests with sample size >= 1.
    """

    goodput_in_bytes_per_sec: float = 0.0
    packet_size: int = 1
    quantile_divisions: int = 100
    data_statistics: DataStatistics = field(default_factory=DataStatistics)
    # Maps returned status to a Counter for easy analyitics.
    statuses: Counter[Status] = field(default_factory=Counter)

    def update(self, time: float, status: Status):
        """Stores calculated data for a given test and updates specific metrics.

        Args:
            time: The RPC goodput time for a given sample.
            status: The RPC result status code for a given sample.

        Returns:
            None.
        """

        self.data_statistics.store(data=time)
        self.statuses[status] += 1
        self.goodput_in_bytes_per_sec = (
            self.packet_size * self.data_statistics.size()
        ) / (self.data_statistics.sum())

    def results(self) -> Iterable[str]:
        """Returns a printable iterable of test results.

        Args:
            None.

        Returns:
            Iterable: An iterable string useful for printing.
        """

        yield "measure_rpc_goodput_statistics test results:"
        yield f"Goodput:                {self.goodput_in_bytes_per_sec} B/s"
        if self.data_statistics.size() > 0:
            # pylint: disable=line-too-long
            yield f"Average roundtrip time: {(self.data_statistics.average() * 1000):.3f} ms"
            yield f"Max roundtrip time:     {(self.data_statistics.max() * 1000):.3f} ms"
            yield f"Median roundtrip time:  {(self.data_statistics.median() * 1000):.3f} ms"
            yield f"Min roundtrip time:     {(self.data_statistics.min() * 1000):.3f} ms"
            yield f"Roundtrip time std dev: {(self.data_statistics.std_dev() * 1000):.3f} ms"
            yield f"Highest (99%) quantile: {((self.data_statistics.quantiles(self.quantile_divisions))[-1] * 1000):.3f} ms"
            # pylint: enable=line-too-long
            yield "Message statuses:"
            total = self.statuses.total()
            for status, count in self.statuses.items():
                # pylint: disable-next=line-too-long
                yield f"  status: {status} count: {count} percent of total: {(count / total ) * 100.0}"
