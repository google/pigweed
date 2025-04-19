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
"""Unit tests for the benchmark class' functions."""
from collections import Counter
from collections.abc import Iterable
import unittest
from unittest import mock

from pw_rpc.benchmark import benchmark
from pw_rpc.benchmark import benchmark_results
from pw_rpc.callback_client.errors import RpcTimeout
from pw_status import Status


def unbounded_search_helper() -> Iterable[Exception | tuple]:
    """A helper class for testing unbounded buffer searches.

    This function returns the pattern of RPC responses for a system with a
    490-byte payload size limit (matching UnaryEcho), with a search starting
    from a buffer size of 0.
    """
    unary_echo = mock.MagicMock()

    # The RPC exception class in errors.py is initializing itself with an
    # f-string of the method name.  Since we are mocking, the f-string init
    # inserts the object reference of the mock into the f-string, instead
    # of the .return_value set for that mock.  This means the exception string
    # is printed incorrectly, so instead we're just overriding the mock's
    # __str__ method so the exceptions behave correctly.
    def workaround(self) -> str:
        del self  # Unused
        return "benchmark.UnaryEcho"

    unary_echo.method.__str__ = workaround
    for size in range(-1, 9):
        # payload sizes: 0 to 256, with range stopping before 512
        yield (Status.OK, 'a' * (0 if 0 > size else 2**size))
    yield RpcTimeout(rpc=unary_echo, timeout=5000)  # payload size: 512
    yield (Status.OK, 'a' * 384)
    yield (Status.OK, 'a' * 448)
    yield (Status.OK, 'a' * 480)
    yield RpcTimeout(rpc=unary_echo, timeout=5000)  # payload size: 496
    yield (Status.OK, 'a' * 488)
    yield RpcTimeout(rpc=unary_echo, timeout=5000)  # payload size: 496
    yield (Status.OK, 'a' * 490)
    yield RpcTimeout(rpc=unary_echo, timeout=5000)  # payload size: 492
    yield RpcTimeout(rpc=unary_echo, timeout=5000)  # payload size: 491


class BenchmarkTests(unittest.TestCase):
    """Tests the functions of the Benchmark class in pw_rpc."""

    def setUp(self) -> None:
        # This is a Magic Mock given the complexity (and multiple layers of
        # objects) required for device instantiation.
        self.rpcs = mock.MagicMock()
        self.default_rpc_result = (Status.OK, 'aaaaaaaa')
        self.rpcs.pw.rpc.Benchmark.UnaryEcho.return_value = (
            self.default_rpc_result
        )
        self.rpcs.pw.rpc.EchoService.Echo.return_value = self.default_rpc_result
        self.unary_echo_max_size_bytes = 490  # Experimentally determined.
        self.echo_max_size_bytes = 64  # Experimentally determined.
        self.benchmark_options = benchmark.BenchmarkOptions(
            max_payload_size=self.echo_max_size_bytes,
            max_sample_set_size=1000,
            quantile_divisions=100,
            use_echo_service=False,
        )
        self.uut = benchmark.Benchmark(
            rpcs=self.rpcs, options=self.benchmark_options
        )

    @mock.patch('time.time', mock.Mock(side_effect=[0, 100]))
    def test_goodput_test_succeeds_with_default_arguments(self) -> None:
        """Verifies that the measure_rpc_goodput function
        will execute correctly if run with default arguments."""
        expected_statistics = benchmark_results.DataStatistics(
            datapoints=[100.0],
        )
        expected_result = benchmark_results.GoodputStatisticsResult(
            data_statistics=expected_statistics,
            goodput_in_bytes_per_sec=0.64,
            packet_size=64,
            statuses=Counter({Status.OK: 1}),
        )

        result = self.uut.measure_rpc_goodput()

        self.assertIsInstance(result, benchmark_results.GoodputStatisticsResult)
        self.assertEqual(
            expected_result.goodput_in_bytes_per_sec,
            result.goodput_in_bytes_per_sec,
        )
        self.assertEqual(
            expected_result.data_statistics.average(),
            result.data_statistics.average(),
        )
        self.assertEqual(
            expected_result.data_statistics.max(),
            result.data_statistics.max(),
        )
        self.assertEqual(
            expected_result.data_statistics.median(),
            result.data_statistics.median(),
        )
        self.assertEqual(
            expected_result.data_statistics.min(),
            result.data_statistics.min(),
        )
        self.assertEqual(
            expected_result.packet_size,
            result.packet_size,
        )
        self.assertEqual(
            expected_result.data_statistics.std_dev(),
            result.data_statistics.std_dev(),
        )
        self.assertEqual(
            expected_result.data_statistics.quantiles(
                divisions=expected_result.quantile_divisions
            ),
            result.data_statistics.quantiles(
                divisions=result.quantile_divisions
            ),
        )
        self.assertEqual(expected_result.statuses, result.statuses)

    @mock.patch(
        'time.time', mock.Mock(side_effect=[100 * x for x in range(0, 2000)])
    )
    def test_goodput_stats_test_succeeds_with_default_arguments(self) -> None:
        """Verifies that the measure_rpc_goodput_statistics function
        will execute correctly if run with default arguments."""
        expected_statistics = benchmark_results.DataStatistics(
            datapoints=[
                100.0
                for x in range(0, benchmark.BenchmarkOptions.quantile_divisions)
            ],
        )
        expected_result = benchmark_results.GoodputStatisticsResult(
            data_statistics=expected_statistics,
            goodput_in_bytes_per_sec=0.64,
            packet_size=64,
            statuses=Counter({Status.OK: 1000}),
        )

        result = self.uut.measure_rpc_goodput_statistics()

        self.assertIsInstance(result, benchmark_results.GoodputStatisticsResult)
        self.assertEqual(
            expected_result.goodput_in_bytes_per_sec,
            result.goodput_in_bytes_per_sec,
        )
        self.assertEqual(
            expected_result.data_statistics.average(),
            result.data_statistics.average(),
        )
        self.assertEqual(
            expected_result.data_statistics.max(),
            result.data_statistics.max(),
        )
        self.assertEqual(
            expected_result.data_statistics.median(),
            result.data_statistics.median(),
        )
        self.assertEqual(
            expected_result.data_statistics.min(),
            result.data_statistics.min(),
        )
        self.assertEqual(
            expected_result.packet_size,
            result.packet_size,
        )
        self.assertEqual(
            expected_result.data_statistics.std_dev(),
            result.data_statistics.std_dev(),
        )
        self.assertEqual(
            expected_result.data_statistics.quantiles(
                divisions=expected_result.quantile_divisions
            ),
            result.data_statistics.quantiles(
                divisions=result.quantile_divisions
            ),
        )
        self.assertEqual(expected_result.statuses, result.statuses)

    @mock.patch(
        'time.time', mock.Mock(side_effect=[100 * x for x in range(0, 1000)])
    )
    def test_goodput_stats_test_succeeds_with_valid_arguments(self) -> None:
        """Verifies that the measure_rpc_goodput_statistics function
        will execute correctly if run with valid, user-supplied arguments."""
        sample_count = 500
        expected_statistics = benchmark_results.DataStatistics(
            datapoints=[100.0 for x in range(1, sample_count)],
        )
        expected_result = benchmark_results.GoodputStatisticsResult(
            data_statistics=expected_statistics,
            goodput_in_bytes_per_sec=0.32,
            packet_size=32,
            statuses=Counter({Status.OK: sample_count}),
        )

        result = self.uut.measure_rpc_goodput_statistics(
            size=32, sample_count=sample_count
        )

        self.assertIsInstance(result, benchmark_results.GoodputStatisticsResult)
        self.assertEqual(
            expected_result.goodput_in_bytes_per_sec,
            result.goodput_in_bytes_per_sec,
        )
        self.assertEqual(
            expected_result.data_statistics.average(),
            result.data_statistics.average(),
        )
        self.assertEqual(
            expected_result.data_statistics.max(),
            result.data_statistics.max(),
        )
        self.assertEqual(
            expected_result.data_statistics.median(),
            result.data_statistics.median(),
        )
        self.assertEqual(
            expected_result.data_statistics.min(),
            result.data_statistics.min(),
        )
        self.assertEqual(
            expected_result.data_statistics.std_dev(),
            result.data_statistics.std_dev(),
        )
        self.assertEqual(
            expected_result.packet_size,
            result.packet_size,
        )
        self.assertEqual(
            expected_result.data_statistics.quantiles(
                divisions=expected_result.quantile_divisions
            ),
            result.data_statistics.quantiles(
                divisions=result.quantile_divisions
            ),
        )
        self.assertEqual(expected_result.statuses, result.statuses)

    def test_goodput_stats_test_errors_on_invalid_size(self) -> None:
        """Verifies that the measure_rpc_goodput_statistics function
        will return an error if given an invalid size argument."""
        self.assertRaises(
            ValueError, self.uut.measure_rpc_goodput_statistics, -1, 1000
        )

    def test_goodput_stats_test_errors_on_invalid_sample_count(self) -> None:
        """Verifies that the measure_rpc_goodput_statistics function
        will return an error if given an invalid sample_count argument."""
        self.assertRaises(
            ValueError, self.uut.measure_rpc_goodput_statistics, 0, -1
        )

    @mock.patch('time.time', mock.Mock(side_effect=[0, 100]))
    def test_goodput_can_use_echo_service_service(self) -> None:
        """Verifies that the measure_rpc_goodput function (and goodput_stats)
        can execute with the default echo service instead of benchmark echo."""

        rpcs = mock.MagicMock()
        rpcs.pw.rpc.EchoService.Echo.return_value = self.default_rpc_result
        expected_statistics = benchmark_results.DataStatistics(
            datapoints=[100.0],
        )
        expected_result = benchmark_results.GoodputStatisticsResult(
            data_statistics=expected_statistics,
            goodput_in_bytes_per_sec=0.64,
            packet_size=64,
            statuses=Counter({Status.OK: 1}),
        )
        options = benchmark.BenchmarkOptions(use_echo_service=True)
        uut = benchmark.Benchmark(rpcs=rpcs, options=options)

        result = uut.measure_rpc_goodput()

        self.assertIsInstance(result, benchmark_results.GoodputStatisticsResult)
        self.assertEqual(
            expected_result.goodput_in_bytes_per_sec,
            result.goodput_in_bytes_per_sec,
        )
        self.assertEqual(
            expected_result.data_statistics.max(),
            result.data_statistics.max(),
        )
        self.assertEqual(
            expected_result.data_statistics.median(),
            result.data_statistics.median(),
        )
        self.assertEqual(
            expected_result.data_statistics.min(),
            result.data_statistics.min(),
        )
        self.assertEqual(
            expected_result.data_statistics.std_dev(),
            result.data_statistics.std_dev(),
        )
        self.assertEqual(
            expected_result.data_statistics.quantiles(
                divisions=expected_result.quantile_divisions
            ),
            result.data_statistics.quantiles(
                divisions=result.quantile_divisions
            ),
        )
        self.assertEqual(expected_result.statuses, result.statuses)

    @mock.patch('builtins.print', autospec=True)
    def test_goodput_statistics_result_prints_when_storing_data(
        self, mock_print
    ) -> None:
        """Verifies that the result of the measure_rpc_goodput_statistics
        function can print populated results to the terminal.
        """
        error_count = 500
        expected_statistics = benchmark_results.DataStatistics(
            datapoints=[100.0],
        )
        result = benchmark_results.GoodputStatisticsResult(
            data_statistics=expected_statistics,
            goodput_in_bytes_per_sec=0.33,
            packet_size=64,
            statuses=Counter(
                {Status.OK: error_count, Status.ABORTED: error_count}
            ),
        )

        result.print_results()

        self.assertEqual(
            mock_print.mock_calls,
            [
                mock.call(
                    "measure_rpc_goodput_statistics test results:", file=None
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Goodput:                {result.goodput_in_bytes_per_sec} B/s",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Average roundtrip time: {(result.data_statistics.average() * 1000):.3f} ms",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Max roundtrip time:     {(result.data_statistics.max() * 1000):.3f} ms",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Median roundtrip time:  {(result.data_statistics.median() * 1000):.3f} ms",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Min roundtrip time:     {(result.data_statistics.min() * 1000):.3f} ms",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Roundtrip time std dev: {(result.data_statistics.std_dev() * 1000):.3f} ms",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Highest (99%) quantile: {(result.data_statistics.quantiles(divisions=result.quantile_divisions)[-1] * 1000):.3f} ms",
                    file=None,
                ),
                mock.call("Message statuses:", file=None),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"  status: {Status.OK} count: {error_count} percent of total: {50.0}",
                    file=None,
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"  status: {Status.ABORTED} count: {error_count} percent of total: {50.0}",
                    file=None,
                ),
            ],
        )

    @mock.patch('builtins.print', autospec=True)
    def test_goodput_statistics_result_prints_just_goodput_on_default_data(
        self, mock_print
    ) -> None:
        """Verifies that the measure_rpc_goodput_statistics print function will
        print empty values and not raise exceptions when run on default values.

        Some functions, like stddev, will raise an exception when called with
        empty arguments (e.g. an empty list of data points).
        """
        result = benchmark_results.GoodputStatisticsResult()

        result.print_results()

        self.assertEqual(
            mock_print.mock_calls,
            [
                mock.call(
                    "measure_rpc_goodput_statistics test results:", file=None
                ),
                mock.call(
                    # pylint: disable-next=line-too-long
                    f"Goodput:                {result.goodput_in_bytes_per_sec} B/s",
                    file=None,
                ),
            ],
        )

    def test_buffer_size_test_succeeds_with_default_arguments(self) -> None:
        """Verifies that the find_max_echo_buffer_size function will execute
        correctly if run with default arguments."""
        expected_result = benchmark_results.FindMaxEchoBufferSizeResult(
            last_error_message="No Errors Raised",
            limited_by_max_size=True,
            max_packet_size_bytes=self.benchmark_options.max_payload_size,
            max_payload_size_bytes=self.benchmark_options.max_payload_size,
            payload_start_size_bytes=0,
        )
        result = self.uut.find_max_echo_buffer_size()

        self.assertIsInstance(
            result, benchmark_results.FindMaxEchoBufferSizeResult
        )
        self.assertEqual(
            expected_result.last_error_message,
            result.last_error_message,
        )
        self.assertEqual(
            expected_result.limited_by_max_size,
            result.limited_by_max_size,
        )
        self.assertEqual(
            expected_result.max_packet_size_bytes,
            result.max_packet_size_bytes,
        )
        self.assertEqual(
            expected_result.max_payload_size_bytes,
            result.max_payload_size_bytes,
        )
        self.assertEqual(
            expected_result.payload_start_size_bytes,
            result.payload_start_size_bytes,
        )

    def test_buffer_size_test_is_right_on_strange_start_value(self) -> None:
        """Verifies that the find_max_echo_buffer_size function will find the
        correct packet size even if given an unusual starting value."""
        expected_result = benchmark_results.FindMaxEchoBufferSizeResult(
            last_error_message="No Errors Raised",
            limited_by_max_size=True,
            max_packet_size_bytes=self.benchmark_options.max_payload_size,
            max_payload_size_bytes=self.benchmark_options.max_payload_size,
            payload_start_size_bytes=3,
        )
        result = self.uut.find_max_echo_buffer_size(start_size=3)

        self.assertIsInstance(
            result, benchmark_results.FindMaxEchoBufferSizeResult
        )
        self.assertEqual(
            expected_result.last_error_message,
            result.last_error_message,
        )
        self.assertEqual(
            expected_result.limited_by_max_size,
            result.limited_by_max_size,
        )
        self.assertEqual(
            expected_result.max_packet_size_bytes,
            result.max_packet_size_bytes,
        )
        self.assertEqual(
            expected_result.max_payload_size_bytes,
            result.max_payload_size_bytes,
        )
        self.assertEqual(
            expected_result.payload_start_size_bytes,
            result.payload_start_size_bytes,
        )

    def test_buffer_size_test_is_right_on_strange_max_size(self) -> None:
        """Verifies that the find_max_echo_buffer_size function will find the
        correct packet size even if given an unusual max packet size."""
        benchmark_options = benchmark.BenchmarkOptions(max_payload_size=65)
        uut = benchmark.Benchmark(rpcs=self.rpcs, options=benchmark_options)
        expected_result = benchmark_results.FindMaxEchoBufferSizeResult(
            last_error_message="No Errors Raised",
            limited_by_max_size=True,
            max_packet_size_bytes=65,
            max_payload_size_bytes=65,
            payload_start_size_bytes=0,
        )
        result = uut.find_max_echo_buffer_size()

        self.assertIsInstance(
            result, benchmark_results.FindMaxEchoBufferSizeResult
        )
        self.assertEqual(
            expected_result.last_error_message,
            result.last_error_message,
        )
        self.assertEqual(
            expected_result.limited_by_max_size,
            result.limited_by_max_size,
        )
        self.assertEqual(
            expected_result.max_packet_size_bytes,
            result.max_packet_size_bytes,
        )
        self.assertEqual(
            expected_result.max_payload_size_bytes,
            result.max_payload_size_bytes,
        )
        self.assertEqual(
            expected_result.payload_start_size_bytes,
            result.payload_start_size_bytes,
        )

    def test_buffer_size_test_succeeds_when_using_echo(self) -> None:
        """Verifies that the find_max_echo_buffer_size function
        will execute correctly if run with the non-unary Echo RPC."""
        benchmark_options = benchmark.BenchmarkOptions(use_echo_service=True)
        uut = benchmark.Benchmark(rpcs=self.rpcs, options=benchmark_options)
        expected_result = benchmark_results.FindMaxEchoBufferSizeResult(
            last_error_message="No Errors Raised",
            # Non-Unary Echo fails because of data loss when decoding, not
            # because of an error returned by the RPC.
            limited_by_max_size=True,
            max_packet_size_bytes=self.benchmark_options.max_payload_size,
            max_payload_size_bytes=self.benchmark_options.max_payload_size,
            payload_start_size_bytes=0,
        )
        result = uut.find_max_echo_buffer_size()

        self.assertIsInstance(
            result, benchmark_results.FindMaxEchoBufferSizeResult
        )
        self.assertEqual(
            expected_result.last_error_message,
            result.last_error_message,
        )
        self.assertEqual(
            expected_result.limited_by_max_size,
            result.limited_by_max_size,
        )
        self.assertEqual(
            expected_result.max_packet_size_bytes,
            result.max_packet_size_bytes,
        )
        self.assertEqual(
            expected_result.max_payload_size_bytes,
            result.max_payload_size_bytes,
        )
        self.assertEqual(
            expected_result.payload_start_size_bytes,
            result.payload_start_size_bytes,
        )

    def test_buffer_size_test_is_right_on_unbounded_search(self) -> None:
        """Verifies that the find_max_echo_buffer_size function will find the
        correct packet size for an unbounded search."""
        benchmark_options = benchmark.BenchmarkOptions(max_payload_size=512)
        rpcs = mock.MagicMock()
        rpcs.pw.rpc.Benchmark.UnaryEcho.side_effect = unbounded_search_helper()
        uut = benchmark.Benchmark(rpcs=rpcs, options=benchmark_options)
        expected_result = benchmark_results.FindMaxEchoBufferSizeResult(
            # pylint: disable-next=line-too-long
            last_error_message="No response received for benchmark.UnaryEcho after 5000 s",
            limited_by_max_size=False,
            max_packet_size_bytes=-1,
            max_payload_size_bytes=self.unary_echo_max_size_bytes,
            payload_start_size_bytes=0,
        )
        result = uut.find_max_echo_buffer_size(max_size=-1)

        self.assertIsInstance(
            result, benchmark_results.FindMaxEchoBufferSizeResult
        )
        self.assertEqual(
            expected_result.last_error_message,
            result.last_error_message,
        )
        self.assertEqual(
            expected_result.limited_by_max_size,
            result.limited_by_max_size,
        )
        self.assertEqual(
            expected_result.max_packet_size_bytes,
            result.max_packet_size_bytes,
        )
        self.assertEqual(
            expected_result.max_payload_size_bytes,
            result.max_payload_size_bytes,
        )
        self.assertEqual(
            expected_result.payload_start_size_bytes,
            result.payload_start_size_bytes,
        )


if __name__ == '__main__':
    unittest.main()
