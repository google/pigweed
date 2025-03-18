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
"""THis class runs various unit tests for the benchmark class"""
from collections import Counter
from dataclasses import dataclass
import unittest
from unittest import mock

from pw_rpc.benchmark import benchmark
from pw_rpc.benchmark import benchmark_results
from pw_status import Status


@dataclass
class TestEchoReturn:
    status = Status.OK


class GoodTimeTest(unittest.TestCase):
    """Tests the Benchmark tools class in pw_rpc."""

    def setUp(self) -> None:
        # This is a Magic Mock given the complexity (and multiple layers of
        # objects) required for device instantiation.
        self.rpcs = mock.MagicMock()
        self.rpcs.pw.rpc.Benchmark.UnaryEcho.return_value = TestEchoReturn()
        self.uut = benchmark.Benchmark(rpcs=self.rpcs)

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
        rpcs.pw.rpc.EchoService.Echo.return_value = TestEchoReturn()
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


if __name__ == '__main__':
    unittest.main()
