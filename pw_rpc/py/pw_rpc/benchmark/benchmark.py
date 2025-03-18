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
"""This module enables RPC benchmarking on a target (server) device."""

from __future__ import annotations

from dataclasses import dataclass
import time
from typing import Any

from pw_rpc.benchmark import benchmark_results


@dataclass
class BenchmarkOptions:
    """Enables customization of the different benchmarking tests."""

    # TODO: https://pwbug.dev/380584579 - Identify all correct defaults for all
    # targets, if possible.
    max_payload_size: int = 64
    max_sample_set_size: int = 1000
    quantile_divisions: int = 100
    use_echo_service: bool = False


class Benchmark:
    """Provides host-side benchmarking capabilities for PW devices"""

    def __init__(
        self,
        rpcs: Any,
        options: BenchmarkOptions = BenchmarkOptions(),
    ) -> None:
        """Creates a benchmarking object for the provided RPCs.

        Args:
            rpcs: The pigweed RPCs to test from the target Device.
            options: Test and device-specific options to set,
                Defaults to a device-specific default if None.

        Returns:
            None: Throws exceptions on error.
        """
        self.rpcs = rpcs
        self.options = options

    def measure_rpc_goodput(
        self, size: int | None = None
    ) -> benchmark_results.GoodputStatisticsResult:
        """Performs a 'goodput' test on the DUT with a variable size data packet

        This test performs a single RPC call and measures its response.  Use the
        ``measure_rpc_goodput_statistics`` function for larger sample sizes.

        Args:
            size: The size, in bytes, of the payload.
                Defaults to max_payload_size if invalid.

        Returns:
            GoodputStatisticsResult: The test statistics collected.
        """
        if size is None:
            size = self.options.max_payload_size
        elif not (0 <= size <= self.options.max_payload_size):
            raise ValueError(
                (
                    f"Invalid {size=}: must be non-negative and"
                    f"less than {self.options.max_payload_size=}"
                )
            )
        result = self.measure_rpc_goodput_statistics(size=size, sample_count=1)

        return result

    def measure_rpc_goodput_statistics(
        self, size: int | None = None, sample_count: int | None = None
    ) -> benchmark_results.GoodputStatisticsResult:
        """Performs a series of 'goodput' calls on the DUT with a variable
           size data packet.

        This test performs a user-specified number of RPC call and measures the
        aggregated statistics for the responses.

        Args:
            size: The size, in bytes, of the payload.
                Defaults to max_payload_size.
            sample_count: The number of samples to collect for the statistics.
                Defaults to max_sample_set_size.

        Returns:
            GoodputStatisticsResult: The test statistics collected.
        """
        if sample_count is None:
            sample_count = self.options.max_sample_set_size
        elif not (0 <= sample_count <= self.options.max_sample_set_size):
            raise ValueError(
                (
                    f"Invalid {sample_count=}: must be non-negative and"
                    f"less than {self.options.max_sample_set_size=}"
                )
            )
        if size is None:
            size = self.options.max_payload_size
        elif not (0 <= size <= self.options.max_payload_size):
            raise ValueError(
                (
                    f"Invalid {size=}: must be non-negative and"
                    f"less than {self.options.max_payload_size=}"
                )
            )
        payload = b"a" * size
        result = benchmark_results.GoodputStatisticsResult(
            packet_size=len(payload),
            quantile_divisions=self.options.quantile_divisions,
        )

        for _ in range(sample_count):
            if self.options.use_echo_service:
                start_time = time.time()
                echo_result = self.rpcs.pw.rpc.EchoService.Echo(msg=payload)
            else:
                start_time = time.time()
                echo_result = self.rpcs.pw.rpc.Benchmark.UnaryEcho(
                    payload=payload
                )
            end_time = time.time()
            result.update(time=end_time - start_time, status=echo_result.status)
        return result
