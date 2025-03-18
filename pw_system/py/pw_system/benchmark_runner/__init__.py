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
# pylint: disable=anomalous-backslash-in-string
"""Pigweed System RPC Benchmark runner.

The ``benchmark_runner`` folder is a convenience tool to execute RPC benchmarks
using the benchmarking tools found in ``pw_rpc``.  It contains\:

#. A host-side runner, ``benchmark_runner.py``, providing a default set of RPC
   Benchmark test cases to execute.

``pw_rpc``'s host-side client is intended to be included in a user's custom
project, while the default runner can be invoked as a python module in a
terminal.

To execute the host-side runner, open two (2) terminals\:

#. In both terminals\:

   #. Navigate to your root-level project folder.
   #. ``source activate.sh``

#. In terminal A\:

   #. bazel run ``//pw_system:simulator_system_example``

#. In terminal B\:
   #. ``python -m pw_system.benchmark_runner -s default --cli``
"""
# pylint: enable=anomalous-backslash-in-string
# The generated protos for this module overlap this `__init__.py` file's import
# namespace, so we need to use extend_path() for them to be discoverable.
# Note: this needs to be done in every nested `__init__.py` file as well (if
# any exist).
from pkgutil import extend_path  # type: ignore

__path__ = extend_path(__path__, __name__)  # type: ignore

from pw_rpc.benchmark.benchmark_results import (
    DataStatistics,
    BaseResult,
    GoodputStatisticsResult,
)
from pw_rpc.benchmark.benchmark import (
    Benchmark,
)
from pw_system.benchmark_runner.benchmark_runner import (
    Runner,
)
