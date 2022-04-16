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
"""Cross-language smoke tests for pw_transfer.

This module runs a subset of the integration tests which are fast and reliable.
It's intended to be run in CQ.
"""

import unittest

from pigweed.pw_transfer.integration_test import cross_language_integration_test


def fast_test_cases():
    """Returns a subset of test cases that are fast enough for CQ."""
    suite = unittest.TestSuite()
    suite.addTest(
        cross_language_integration_test.PwTransferIntegrationTest(
            'test_small_client_write_0_cpp'))
    suite.addTest(
        cross_language_integration_test.PwTransferIntegrationTest(
            'test_small_client_write_1_java'))
    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='fast_test_cases')
