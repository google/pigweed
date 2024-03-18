#!/usr/bin/env python3
# Copyright 2021 The Pigweed Authors
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
"""Tests using the callback client for pw_rpc."""

import contextlib
import unittest

import pw_hdlc.rpc
from pw_rpc import benchmark_pb2, testing
from pw_status import Status

ITERATIONS = 50


class RpcIntegrationTest(unittest.TestCase):
    """Calls RPCs on an RPC server through a socket."""

    test_server_command: tuple[str, ...] = ()
    port: int

    def setUp(self) -> None:
        self._context = pw_hdlc.rpc.HdlcRpcLocalServerAndClient(
            self.test_server_command, self.port, [benchmark_pb2]
        )
        self.rpcs = self._context.client.channel(1).rpcs

    def tearDown(self) -> None:
        self._context.close()

    def test_unary(self) -> None:
        for i in range(ITERATIONS):
            payload = f'O_o #{i}'.encode()
            status, reply = self.rpcs.pw.rpc.Benchmark.UnaryEcho(
                payload=payload
            )
            self.assertIs(status, Status.OK)
            self.assertEqual(reply.payload, payload)

    def test_bidirectional(self) -> None:
        with self.rpcs.pw.rpc.Benchmark.BidirectionalEcho.invoke() as call:
            responses = call.get_responses()

            for i in range(ITERATIONS):
                payload = f'O_o #{i}'.encode()
                call.send(benchmark_pb2.Payload(payload=payload))

                self.assertEqual(next(responses).payload, payload)

    def test_bidirectional_call_twice(self) -> None:
        rpc = self.rpcs.pw.rpc.Benchmark.BidirectionalEcho

        for _ in range(ITERATIONS):
            with contextlib.ExitStack() as stack:
                first_call = stack.enter_context(rpc.invoke())
                first_call_responses = first_call.get_responses()
                first_call.send(payload=b'abc')
                self.assertEqual(
                    next(first_call_responses), rpc.response(payload=b'abc')
                )
                self.assertFalse(first_call.completed())

                second_call = stack.enter_context(rpc.invoke())
                second_call_responses = second_call.get_responses()
                second_call.send(payload=b'123')
                self.assertEqual(
                    next(second_call_responses), rpc.response(payload=b'123')
                )
                self.assertFalse(second_call.completed())

                # Check that issuing `second_call` did not cancel `first call`.
                self.assertFalse(first_call.completed())
                self.assertIs(first_call.error, None)

                # Send to `first_call` again and check for a response.
                first_call.send(payload=b'def')
                self.assertEqual(
                    next(first_call_responses), rpc.response(payload=b'def')
                )


def _main(
    test_server_command: list[str], port: int, unittest_args: list[str]
) -> None:
    RpcIntegrationTest.test_server_command = tuple(test_server_command)
    RpcIntegrationTest.port = port
    unittest.main(argv=unittest_args)


if __name__ == '__main__':
    _main(**vars(testing.parse_test_server_args()))
