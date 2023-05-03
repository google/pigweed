# Copyright 2023 The Pigweed Authors
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

"""Log decoder tests."""

import logging
from typing import List
from unittest import TestCase, main

from pw_log.log_decoder import (
    Log,
    LogStreamDecoder,
    pw_status_code_to_name,
    timestamp_parser_ns_since_boot,
)
from pw_log.proto import log_pb2
import pw_tokenizer

_MESSAGE_NO_FILENAME = 'World'
_MESSAGE_AND_ARGS_NO_FILENAME = f'■msg♦{_MESSAGE_NO_FILENAME}'
_MESSAGE_TOKEN_NO_FILENAME = pw_tokenizer.tokens.pw_tokenizer_65599_hash(
    _MESSAGE_AND_ARGS_NO_FILENAME
)

# Creating a database with tokenized information for the core to detokenize
# tokenized log entries.
_TOKEN_DATABASE = pw_tokenizer.tokens.Database(
    [
        pw_tokenizer.tokens.TokenizedStringEntry(0x01148A48, 'total_dropped'),
        pw_tokenizer.tokens.TokenizedStringEntry(
            0x03796798, 'min_queue_remaining'
        ),
        pw_tokenizer.tokens.TokenizedStringEntry(0x2E668CD6, 'Jello, world!'),
        pw_tokenizer.tokens.TokenizedStringEntry(0x329481A2, 'parser_errors'),
        pw_tokenizer.tokens.TokenizedStringEntry(0x7F35A9A5, 'TestName'),
        pw_tokenizer.tokens.TokenizedStringEntry(0xCC6D3131, 'Jello?'),
        pw_tokenizer.tokens.TokenizedStringEntry(
            0x144C501D, '■msg♦SampleMessage■module♦MODULE■file♦file/path.cc'
        ),
        pw_tokenizer.tokens.TokenizedStringEntry(0x0000106A, 'ModuleOrMessage'),
        pw_tokenizer.tokens.TokenizedStringEntry(
            pw_tokenizer.tokens.pw_tokenizer_65599_hash(
                '■msg♦World■module♦wifi■file♦/path/to/file.cc'
            ),
            '■msg♦World■module♦wifi■file♦/path/to/file.cc',
        ),
        pw_tokenizer.tokens.TokenizedStringEntry(
            _MESSAGE_TOKEN_NO_FILENAME, _MESSAGE_AND_ARGS_NO_FILENAME
        ),
    ]
)
_DETOKENIZER = pw_tokenizer.Detokenizer(_TOKEN_DATABASE)


def _create_log_entry_with_tokenized_fields(
    message: str, module: str, file: str, thread: str, line: int, level: int
) -> log_pb2.LogEntry:
    """Tokenizing tokenizable LogEntry fields to become a detoknized log."""
    tokenized_message = pw_tokenizer.encode.encode_token_and_args(
        pw_tokenizer.tokens.pw_tokenizer_65599_hash(message)
    )
    tokenized_module = pw_tokenizer.encode.encode_token_and_args(
        pw_tokenizer.tokens.pw_tokenizer_65599_hash(module)
    )
    tokenized_file = pw_tokenizer.encode.encode_token_and_args(
        pw_tokenizer.tokens.pw_tokenizer_65599_hash(file)
    )
    tokenized_thread = pw_tokenizer.encode.encode_token_and_args(
        pw_tokenizer.tokens.pw_tokenizer_65599_hash(thread)
    )

    return log_pb2.LogEntry(
        message=tokenized_message,
        module=tokenized_module,
        file=tokenized_file,
        line_level=Log.pack_line_level(line, level),
        thread=tokenized_thread,
    )


class TestLogStreamDecoder(TestCase):
    """Tests LogStreamDecoder functionality."""

    def setUp(self) -> None:
        """Set up logs decoder."""

        def parse_pw_status(msg: str) -> str:
            return pw_status_code_to_name(msg)

        self._logs: List[Log] = []

        def decoded_log_handler(log: Log) -> None:
            self._logs.append(log)

        self.decoder = LogStreamDecoder(
            decoded_log_handler=decoded_log_handler,
            detokenizer=_DETOKENIZER,
            source_name='source',
            timestamp_parser=timestamp_parser_ns_since_boot,
            message_parser=parse_pw_status,
        )

    def test_parse_log_entry_valid_non_tokenized(self) -> None:
        """Test that valid LogEntry protos are parsed correctly."""
        expected_log = Log(
            message='Hello',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Hello',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(result, expected_log)

    def test_parse_log_entry_valid_packed_message(self) -> None:
        """Test that valid LogEntry protos are parsed correctly."""
        log_with_metadata_in_message = Log(
            message='World',
            file_and_line='/path/to/file.cc',
            level=logging.DEBUG,
            source_name=self.decoder.source_name,
            module_name='wifi',
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=bytes(
                    '■msg♦World■module♦wifi■file♦/path/to/file.cc'.encode(
                        'utf-8'
                    )
                ),
                line_level=Log.pack_line_level(0, logging.DEBUG),
                timestamp=100,
            )
        )
        self.assertEqual(result, log_with_metadata_in_message)

    def test_parse_log_entry_valid_logs_drop_message(self) -> None:
        """Test that valid LogEntry protos are parsed correctly."""
        dropped_message = Log(
            message='Dropped 30 logs due to buffer too small',
            level=logging.WARNING,
            source_name=self.decoder.source_name,
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(message=b'buffer too small', dropped=30)
        )
        self.assertEqual(result, dropped_message)

    def test_parse_log_entry_valid_tokenized(self) -> None:
        """Test that tokenized LogEntry protos are parsed correctly."""
        message = 'Jello, world!'
        module_name = 'TestName'
        file = 'parser_errors'
        thread_name = 'Jello?'
        line = 123
        level = logging.INFO

        expected_log = Log(
            message=message,
            module_name=module_name,
            file_and_line=file + ':123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
            thread_name=thread_name,
        )

        log_entry = _create_log_entry_with_tokenized_fields(
            message, module_name, file, thread_name, line, level
        )
        result = self.decoder.parse_log_entry_proto(log_entry)
        self.assertEqual(result, expected_log, msg='Log was not detokenized')

    def test_tokenized_contents_not_detokenized(self):
        """Test fields with tokens not in the database are not decrypted."""
        # The following strings do not have tokens in the device token db.
        message_not_in_db = 'device is shutting down.'
        module_name_not_in_db = 'Battery'
        file_not_in_db = 'charger.cc'
        thread_name_not_in_db = 'BatteryStatus'
        line = 123
        level = logging.INFO

        log_entry = _create_log_entry_with_tokenized_fields(
            message_not_in_db,
            module_name_not_in_db,
            file_not_in_db,
            thread_name_not_in_db,
            line,
            level,
        )
        message = pw_tokenizer.proto.decode_optionally_tokenized(
            _DETOKENIZER, log_entry.message
        )
        module = pw_tokenizer.proto.decode_optionally_tokenized(
            _DETOKENIZER, log_entry.module
        )
        file = pw_tokenizer.proto.decode_optionally_tokenized(
            _DETOKENIZER, log_entry.file
        )
        thread = pw_tokenizer.proto.decode_optionally_tokenized(
            _DETOKENIZER, log_entry.thread
        )
        expected_log = Log(
            message=message,
            module_name=module,
            file_and_line=file + ':123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
            thread_name=thread,
        )
        result = self.decoder.parse_log_entry_proto(log_entry)
        self.assertEqual(
            result, expected_log, msg='Log was unexpectedly detokenized'
        )

    def test_extracting_log_entry_fields_from_tokenized_metadata(self):
        """Test that tokenized metadata can be used to extract other fields."""
        metadata = '■msg♦World■module♦wifi■file♦/path/to/file.cc'
        thread_name = 'M0Log'

        log_entry = log_pb2.LogEntry(
            message=pw_tokenizer.encode.encode_token_and_args(
                pw_tokenizer.tokens.pw_tokenizer_65599_hash(metadata)
            ),
            line_level=Log.pack_line_level(0, logging.DEBUG),
            thread=bytes(thread_name.encode('utf-8')),
        )

        log_with_metadata_in_message = Log(
            message='World',
            file_and_line='/path/to/file.cc',
            level=logging.DEBUG,
            source_name=self.decoder.source_name,
            module_name='wifi',
            timestamp='0:00',
            thread_name=thread_name,
        )

        result = self.decoder.parse_log_entry_proto(log_entry)
        self.assertEqual(
            result, log_with_metadata_in_message, msg='Log was detokenized.'
        )

    def test_extracting_status_argument_from_log_message(self):
        """Test extract status from log message."""
        expected_log = Log(
            message='Could not start flux capacitor: PERMISSION_DENIED',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Status=7',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

        expected_log = Log(
            message='Error connecting to server: UNAVAILABLE',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Error connecting to server: pw::Status=14',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_extracting_status_with_improper_spacing(self):
        """Test spaces before pw::Status are ignored."""
        expected_log = Log(
            message='Error connecting to server:UNAVAILABLE',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )

        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Error connecting to server:pw::Status=14',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

        expected_log = Log(
            message='Error connecting to server:    UNAVAILABLE',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )

        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Error connecting to server:    pw::Status=14',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_not_extracting_status_extra_space_before_code(self):
        """Test spaces after pw::Status are not allowed."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::Status=    7',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )

        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Status=    7',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_not_extracting_status_new_line_before_code(self):
        """Test new line characters after pw::Status are not allowed."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::Status=\n7',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Status=\n7',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_not_extracting_status_from_log_message_with_improper_format(self):
        """Test status not extracted from log message with incorrect format."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::Status12',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Status12',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_status_code_in_message_does_not_exist(self):
        """Test status does not exist in pw_status."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::Status=17',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Status=17',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_status_code_in_message_is_negative(self):
        """Test status code is negative."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::Status=-1',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Status=-1',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_status_code_is_name(self):
        """Test if the status code format includes the name instead."""
        expected_log = Log(
            message='Cannot use flux capacitor: pw::Status=PERMISSION_DENIED',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=(
                    b'Cannot use flux capacitor: pw::Status=PERMISSION_DENIED'
                ),
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_spelling_mistakes_with_status_keyword(self):
        """Test spelling mistakes with status keyword."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::Rtatus=12',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::Rtatus=12',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_spelling_mistakes_with_status_keyword_lowercase_s(self):
        """Test spelling mistakes with status keyword."""
        expected_log = Log(
            message='Could not start flux capacitor: pw::status=13',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Could not start flux capacitor: pw::status=13',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_status_code_at_beginning_of_message(self):
        """Test embedded status argument is found."""
        expected_log = Log(
            message='UNAVAILABLE to connect to server.',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'pw::Status=14 to connect to server.',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_status_code_in_the_middle_of_message(self):
        """Test embedded status argument is found."""
        expected_log = Log(
            message='Connection error: UNAVAILABLE connecting to server.',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=(
                    b'Connection error: pw::Status=14 connecting to server.'
                ),
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_status_code_with_no_surrounding_spaces(self):
        """Test embedded status argument is found."""
        expected_log = Log(
            message='Connection error:UNAVAILABLEconnecting to server.',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Connection error:pw::Status=14connecting to server.',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_multiple_status_arguments_in_log_message(self):
        """Test replacement of multiple status arguments into status string."""
        expected_log = Log(
            message='Connection error: UNAVAILABLE and PERMISSION_DENIED.',
            file_and_line='my/path/file.cc:123',
            level=logging.INFO,
            source_name=self.decoder.source_name,
            timestamp='0:00',
        )
        result = self.decoder.parse_log_entry_proto(
            log_pb2.LogEntry(
                message=b'Connection error: pw::Status=14 and pw::Status=7.',
                file=b'my/path/file.cc',
                line_level=Log.pack_line_level(123, logging.INFO),
            )
        )
        self.assertEqual(
            result,
            expected_log,
            msg='Status was not extracted from log message.',
        )

    def test_no_filename_in_message_parses_successfully(self):
        """Test that if the file name is not present the log entry is parsed."""
        thread_name = 'thread'

        log_entry = log_pb2.LogEntry(
            message=pw_tokenizer.encode.encode_token_and_args(
                _MESSAGE_TOKEN_NO_FILENAME
            ),
            line_level=Log.pack_line_level(123, logging.DEBUG),
            thread=bytes(thread_name.encode('utf-8')),
        )
        expected_log = Log(
            message=_MESSAGE_NO_FILENAME,
            file_and_line=':123',
            level=logging.DEBUG,
            source_name=self.decoder.source_name,
            timestamp='0:00',
            thread_name=thread_name,
        )
        result = self.decoder.parse_log_entry_proto(log_entry)
        self.assertEqual(result, expected_log)


if __name__ == '__main__':
    main()
