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
from pw_rpc.callback_client.errors import RpcTimeout, RpcError
from pw_status import Status


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
                status, _ = self.rpcs.pw.rpc.EchoService.Echo(msg=payload)
            else:
                start_time = time.time()
                status, _ = self.rpcs.pw.rpc.Benchmark.UnaryEcho(
                    payload=payload
                )
            end_time = time.time()
            result.update(time=end_time - start_time, status=status)
        return result

    def find_max_echo_buffer_size(
        self, start_size: int = 0, max_size: int | None = None
    ) -> benchmark_results.FindMaxEchoBufferSizeResult:
        """Uses a doubling search to find the maximum packet size the Echo
           message supports.

        This test sends increasingly larger packets to determine the maximum
        payload size the target server can accept.  It will attempt to find
        the exact byte size of the maximum packet, but may only have accuracy
        to the nearest doubling size if the server crashes.

        Args:
            start_size: The initial payload size, in bytes.
                Defaults to 0.
            max_size: The maximum size to send for a payload.
                Defaults to max_sample_set_size.  Disables the max_size check
                if negative (and runs until an error is raised).

        Returns:
            FindMaxEchoBufferSizeResult: The test results.
        """
        if max_size is None:
            max_size = self.options.max_payload_size

        if 0 > start_size:
            raise ValueError((f"Invalid {start_size=}: must be non-negative"))
        if 0 <= max_size < start_size:
            raise ValueError(
                (f"Invalid {start_size=}: must be less than {max_size=}")
            )

        diff_size = 0
        last_error = "No Errors Raised"
        last_size = start_size
        limited_by_max_size = False
        near_max_size = False
        payload_size = start_size
        result_ok = False

        while True:
            payload = b"a" * payload_size
            # This code isn't calling measure_rpc_goodput_statistics because
            # that function validates max_size, which this test potentially
            # wants to let scale unbounded.  Also it's nicer to just fill in
            # the return object directly instead of having to convert the
            # output of the statistics result into this functions' return type.
            try:
                if self.options.use_echo_service:
                    status, _ = self.rpcs.pw.rpc.EchoService.Echo(msg=payload)
                else:
                    status, _ = self.rpcs.pw.rpc.Benchmark.UnaryEcho(
                        payload=payload
                    )
                result_ok = True
            # The lower layers can still raise RuntimeError or other kinds of
            # exceptions, which we should treat as non-benchmarking errors.
            except (ValueError, RpcTimeout, RpcError) as err:
                last_error = str(err)
                result_ok = False

            # The default benchmark will raise a DATA LOSS exception but return
            # Status.OK from the RPC.
            if result_ok and status == Status.OK:
                last_size = payload_size

                if (0 < max_size) and (last_size == max_size):
                    limited_by_max_size = True
                    break

                # If already near the limit, prevent thrashing around the max
                # size by adding half the distance from the last packet to the
                # max until it fails.  Otherwise scale as normal.
                if near_max_size:
                    diff_size = diff_size // 2

                    if 0 == diff_size:
                        break
                    payload_size += diff_size
                else:
                    payload_size = 1 if 0 == payload_size else payload_size * 2

                if 0 < max_size < payload_size:
                    payload_size = max_size
                    limited_by_max_size = True
            else:
                if last_size == payload_size:
                    break
                near_max_size = True
                # Round down to the nearest integer.
                diff_size = (payload_size - last_size) // 2
                payload_size -= max(1, diff_size)

        return benchmark_results.FindMaxEchoBufferSizeResult(
            max_packet_size_bytes=max_size,
            payload_start_size_bytes=start_size,
            max_payload_size_bytes=payload_size,
            last_error_message=last_error,
            limited_by_max_size=limited_by_max_size,
        )
