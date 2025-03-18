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
"""Pigweed RPC Benchmark tools.

The ``benchmark`` folder contains a few utilities\:

#. A host-side client, ``benchmark.py``, containing functions for basic RPC
   statistics gathering and measurement.
#. An output definition file, ``benchmark_results.py``, used to record tests
   results and enable extending test data.

The host-side client is intended to be included in a user's custom project,
while the default runner, implemented in ``pw_system``, can be invoked as a
python module in a terminal.

The default runner has been implemented in the ``pw_system`` module.  It can
be used as a metric of its own or as a starting implementation for users to
make their own benchmarking runner.

See the ``pw_system`` folder for details on the default runner.
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
