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
"""Cross-language pw_transfer tests that are as small/fast as possible.

Usage:

   bazel run pw_transfer/integration_test:cross_language_small_test

Command-line arguments must be provided after a double-dash:

   bazel run pw_transfer/integration_test:cross_language_small_test -- \
       --server-port 3304

Which tests to run can be specified as command-line arguments:

  bazel run pw_transfer/integration_test:cross_language_small_test -- \
      SmallTransferIntegrationTest.test_single_byte_client_write_1_java

"""

from parameterized import parameterized

import test_fixture
from test_fixture import TransferIntegrationTestHarness


class SmallTransferIntegrationTest(test_fixture.TransferIntegrationTest):
    # Each set of transfer tests uses a different client/server port pair to
    # allow tests to be run in parallel.
    HARNESS_CONFIG = TransferIntegrationTestHarness.Config(server_port=3302,
                                                           client_port=3303)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_empty_client_write(self, client_type):
        payload = b""
        config = self.default_config()
        resource_id = 5
        self.do_single_write(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_null_byte_client_write(self, client_type):
        payload = b"\0"
        config = self.default_config()
        resource_id = 5
        self.do_single_write(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_single_byte_client_write(self, client_type):
        payload = b"?"
        config = self.default_config()
        resource_id = 5
        self.do_single_write(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_small_client_write(self, client_type):
        payload = b"some data"
        config = self.default_config()
        resource_id = 5
        self.do_single_write(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_empty_client_read(self, client_type):
        payload = b""
        config = self.default_config()
        resource_id = 5
        self.do_single_read(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_null_byte_client_read(self, client_type):
        payload = b"\0"
        config = self.default_config()
        resource_id = 5
        self.do_single_read(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_single_byte_client_read(self, client_type):
        payload = b"?"
        config = self.default_config()
        resource_id = 5
        self.do_single_read(client_type, config, resource_id, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
        ("python"),
    ])
    def test_small_client_read(self, client_type):
        payload = b"some data"
        config = self.default_config()
        resource_id = 5
        self.do_single_read(client_type, config, resource_id, payload)


if __name__ == '__main__':
    test_fixture.run_tests_for(SmallTransferIntegrationTest)
