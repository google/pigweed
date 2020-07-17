#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Tests creating pw_rpc client."""

import unittest

from pw_rpc import packets

_TEST_REQUEST = packets.RpcPacket(
    type=packets.PacketType.RPC,
    channel_id=1,
    service_id=2,
    method_id=3,
    payload=packets.RpcPacket(status=321).SerializeToString())


class PacketsTest(unittest.TestCase):
    def test_encode_request(self):
        data = packets.encode_request((1, 2, 3), packets.RpcPacket(status=321))
        packet = packets.RpcPacket()
        packet.ParseFromString(data)

        self.assertEqual(_TEST_REQUEST, packet)

    def test_encode_cancel(self):
        data = packets.encode_cancel((9, 8, 7))

        packet = packets.RpcPacket()
        packet.ParseFromString(data)

        self.assertEqual(
            packet,
            packets.RpcPacket(type=packets.PacketType.CANCEL,
                              channel_id=9,
                              service_id=8,
                              method_id=7))

    def test_decode(self):
        self.assertEqual(_TEST_REQUEST,
                         packets.decode(_TEST_REQUEST.SerializeToString()))


if __name__ == '__main__':
    unittest.main()
