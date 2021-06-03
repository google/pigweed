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
"""Tests decoding a proto with tokenized fields."""

import unittest

from pw_tokenizer_tests.detokenize_proto_test_pb2 import TheMessage

from pw_tokenizer import detokenize, encode, tokens
from pw_tokenizer.proto import detokenize_fields

_DATABASE = tokens.Database(
    [tokens.TokenizedStringEntry(0xAABBCCDD, "Luke, we're gonna have %s")])
_DETOKENIZER = detokenize.Detokenizer(_DATABASE)


class TestDetokenizeProtoFields(unittest.TestCase):
    """Tests detokenizing optionally tokenized proto fields."""
    def test_plain_text(self) -> None:
        proto = TheMessage(message=b'boring conversation anyway!')
        detokenize_fields(_DETOKENIZER, proto)
        self.assertEqual(proto.message, b'boring conversation anyway!')

    def test_binary(self) -> None:
        proto = TheMessage(message=b'\xDD\xCC\xBB\xAA\x07company')
        detokenize_fields(_DETOKENIZER, proto)
        self.assertEqual(proto.message, b"Luke, we're gonna have company")

    def test_base64(self) -> None:
        base64 = encode.prefixed_base64(b'\xDD\xCC\xBB\xAA\x07company')
        proto = TheMessage(message=base64.encode())
        detokenize_fields(_DETOKENIZER, proto)
        self.assertEqual(proto.message, b"Luke, we're gonna have company")

    def test_plain_text_with_prefixed_base64(self) -> None:
        base64 = encode.prefixed_base64(b'\xDD\xCC\xBB\xAA\x09pancakes!')
        proto = TheMessage(message=f'Good morning, {base64}'.encode())
        detokenize_fields(_DETOKENIZER, proto)
        self.assertEqual(proto.message,
                         b"Good morning, Luke, we're gonna have pancakes!")

    def test_unknown_token_not_utf8(self) -> None:
        proto = TheMessage(message=b'\xFE\xED\xF0\x0D')
        detokenize_fields(_DETOKENIZER, proto)
        self.assertEqual(proto.message.decode(),
                         encode.prefixed_base64(b'\xFE\xED\xF0\x0D'))

    def test_only_control_characters(self) -> None:
        proto = TheMessage(message=b'\1\2\3\4')
        detokenize_fields(_DETOKENIZER, proto)
        self.assertEqual(proto.message.decode(),
                         encode.prefixed_base64(b'\1\2\3\4'))


if __name__ == '__main__':
    unittest.main()
