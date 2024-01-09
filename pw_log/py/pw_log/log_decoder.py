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

"""Utils to decode logs."""

from dataclasses import dataclass
import datetime
import logging
import re
from typing import Any, Callable, Dict, Optional, Union

from pw_log.proto import log_pb2
import pw_log_tokenized
import pw_status
from pw_tokenizer import Detokenizer
from pw_tokenizer.proto import decode_optionally_tokenized

_LOG = logging.getLogger(__name__)


@dataclass(frozen=True)
class LogLineLevel:
    """Tuple of line number and level packed in LogEntry."""

    line: int
    level: int


class Log:
    """A decoded, human-readable representation of a LogEntry.

    Contains fields to represent a decoded pw_log/log.proto LogEntry message in
    a human readable way.

    Attributes:
        message: The log message as a string.
        level: A integer representing the log level, follows logging levels.
        flags: An integer with the bit flags.
        timestamp: A string representation of a timestamp.
        module_name: The module name as a string.
        thread_name: The thread name as a string.
        source_name: The source name as a string.
        file_and_line: The filepath and line as a string.
        metadata_fields: Extra fields with string-string mapping.
    """

    _LOG_LEVEL_NAMES = {
        logging.DEBUG: 'DBG',
        logging.INFO: 'INF',
        logging.WARNING: 'WRN',
        logging.ERROR: 'ERR',
        logging.CRITICAL: 'CRT',
        # The logging module does not have a FATAL level. This module's log
        # level values are 10 * PW_LOG_LEVEL values. PW_LOG_LEVEL_FATAL is 7.
        70: 'FTL',
    }
    _LEVEL_MASK = 0x7  # pylint: disable=C0103
    _LINE_OFFSET = 3  # pylint: disable=C0103

    def __init__(
        self,
        message: str = '',
        level: int = logging.NOTSET,
        flags: int = 0,
        timestamp: str = '',
        module_name: str = '',
        thread_name: str = '',
        source_name: str = '',
        file_and_line: str = '',
        metadata_fields: Optional[Dict[str, str]] = None,
    ) -> None:
        self.message = message
        self.level = level  # Value from logging levels.
        self.flags = flags
        self.timestamp = timestamp  # A human readable value.
        self.module_name = module_name
        self.thread_name = thread_name
        self.source_name = source_name
        self.file_and_line = file_and_line
        self.metadata_fields = dict()
        if metadata_fields:
            self.metadata_fields.update(metadata_fields)

    def __eq__(self, other: Any) -> bool:
        if not isinstance(other, Log):
            return False

        return (
            self.message == other.message
            and self.level == other.level
            and self.flags == other.flags
            and self.timestamp == other.timestamp
            and self.module_name == other.module_name
            and self.thread_name == other.thread_name
            and self.source_name == other.source_name
            and self.file_and_line == other.file_and_line
            and self.metadata_fields == other.metadata_fields
        )

    def __repr__(self) -> str:
        return self.__str__()

    def __str__(self) -> str:
        level_name = self._LOG_LEVEL_NAMES.get(self.level, '')
        metadata = ' '.join(map(str, self.metadata_fields.values()))
        return (
            f'{level_name} [{self.source_name}] {self.module_name} '
            f'{self.timestamp} {self.message} {self.file_and_line} '
            f'{metadata}'
        ).strip()

    @staticmethod
    def pack_line_level(line: int, logging_log_level: int) -> int:
        """Packs the line and level values into an integer as done in LogEntry.

        Args:
            line: the line number
            level: the logging level using logging levels.
        Returns:
            An integer with the two values bitpacked.
        """
        return (line << Log._LINE_OFFSET) | (
            Log.logging_level_to_pw_log_level(logging_log_level)
            & Log._LEVEL_MASK
        )

    @staticmethod
    def unpack_line_level(packed_line_level: int) -> LogLineLevel:
        """Unpacks the line and level values packed as done in LogEntry.

        Args:
            An integer with the two values bitpacked.
        Returns:
            line: the line number
            level: the logging level using logging levels.
        """
        line = packed_line_level >> Log._LINE_OFFSET
        # Convert to logging module level number.
        level = Log.pw_log_level_to_logging_level(
            packed_line_level & Log._LEVEL_MASK
        )
        return LogLineLevel(line=line, level=level)

    @staticmethod
    def pw_log_level_to_logging_level(pw_log_level: int) -> int:
        """Maps a pw_log/levels.h value to Python logging level value."""
        return pw_log_level * 10

    @staticmethod
    def logging_level_to_pw_log_level(logging_log_level: int) -> int:
        """Maps a Python logging level value to a pw_log/levels.h value."""
        return int(logging_log_level / 10)


def pw_status_code_to_name(
    message: str, status_pattern: str = 'pw::Status=([0-9]+)'
) -> str:
    """Replaces the pw::Status code number with the status name.

    Searches for a matching pattern encapsulating the status code number and
    replaces it with the status name. This is useful when logging the status
    code instead of the string to save space.

    Args:
        message: The string to look for the status code.
        status_pattern: The regex pattern describing the format encapsulating
          the status code.
    Returns:
       The message string with the status name if found, else the original
       string.
    """
    # Min and max values for Status.
    max_status_value = max(status.value for status in pw_status.Status)
    min_status_value = min(status.value for status in pw_status.Status)

    def replacement_callback(match: re.Match) -> str:
        status_code = int(match.group(1))
        if min_status_value <= status_code <= max_status_value:
            return pw_status.Status(status_code).name
        return match.group(0)

    return re.sub(
        pattern=status_pattern, repl=replacement_callback, string=message
    )


def log_decoded_log(
    log: Log, logger: Union[logging.Logger, logging.LoggerAdapter]
) -> None:
    """Formats and saves the log information in a pw console compatible way.

    Arg:
        logger: The logger to emit the log information to.
    """
    # Fields used for pw console table view.
    log.metadata_fields['module'] = log.module_name
    log.metadata_fields['source_name'] = log.source_name
    log.metadata_fields['timestamp'] = log.timestamp
    log.metadata_fields['msg'] = log.message
    log.metadata_fields['file'] = log.file_and_line

    logger.log(
        log.level,
        '[%s] %s %s %s %s',
        log.source_name,
        log.module_name,
        log.timestamp,
        log.message,
        log.file_and_line,
        extra=dict(extra_metadata_fields=log.metadata_fields),
    )


def timestamp_parser_ns_since_boot(timestamp: int) -> str:
    """Decodes timestamp as nanoseconds since boot.

    Args:
        timestamp: The timestamp as an integer.
    Returns:
        A string representation of the timestamp.
    """
    return str(datetime.timedelta(seconds=timestamp / 1e9))[:-3]


class LogStreamDecoder:
    """Decodes an RPC stream of LogEntries packets.

    Performs log drop detection on the stream of LogEntries proto messages.

    Args:
        decoded_log_handler: Callback called on each decoded log.
        detokenizer: Detokenizes log messages if tokenized when provided.
        source_name: Optional string to identify the logs source.
        timestamp_parser: Optional timestamp parser number to a string.
        message_parser: Optional message parser called after detokenization is
          attempted on a log message.
    """

    DROP_REASON_LOSS_AT_TRANSPORT = 'loss at transport'
    DROP_REASON_SOURCE_NOT_CONNECTED = 'source not connected'
    DROP_REASON_SOURCE_ENQUEUE_FAILURE = 'enqueue failure at source'

    def __init__(
        self,
        decoded_log_handler: Callable[[Log], None],
        detokenizer: Optional[Detokenizer] = None,
        source_name: str = '',
        timestamp_parser: Optional[Callable[[int], str]] = None,
        message_parser: Optional[Callable[[str], str]] = None,
    ):
        self.decoded_log_handler = decoded_log_handler
        self.detokenizer = detokenizer
        self.source_name = source_name
        self.timestamp_parser = timestamp_parser
        self.message_parser = message_parser
        self._expected_log_sequence_id = 0

    def parse_log_entries_proto(
        self, log_entries_proto: log_pb2.LogEntries
    ) -> None:
        """Parses each LogEntry in log_entries_proto.

        Args:
            log_entry_proto: A LogEntry message proto.
        Returns:
            A Log object with the decoded log_entry_proto.
        """
        has_received_logs = self._expected_log_sequence_id > 0
        dropped_log_count = self._calculate_dropped_logs(log_entries_proto)
        if dropped_log_count > 0:
            reason = (
                self.DROP_REASON_LOSS_AT_TRANSPORT
                if has_received_logs
                else self.DROP_REASON_SOURCE_NOT_CONNECTED
            )
            self.decoded_log_handler(
                self._handle_log_drop_count(dropped_log_count, reason)
            )
        elif dropped_log_count < 0:
            _LOG.error('Log sequence ID is smaller than expected')

        for i, log_entry_proto in enumerate(log_entries_proto.entries):
            # Handle dropped count first.
            if log_entry_proto.dropped:
                # Avoid duplicating drop reports since the device will report
                # a drop count due to a transmission failure, of the last
                # attempted transmission only, in the first entry of the next
                # successful transmission.
                if i == 0 and dropped_log_count >= log_entry_proto.dropped:
                    continue
            parsed_log = self.parse_log_entry_proto(log_entry_proto)
            self.decoded_log_handler(parsed_log)

    def parse_log_entry_proto(self, log_entry_proto: log_pb2.LogEntry) -> Log:
        """Parses the log_entry_proto contents into a human readable format.

        Args:
            log_entry_proto: A LogEntry message proto.
        Returns:
            A Log object with the decoded log_entry_proto.
        """
        detokenized_message = self._decode_optionally_tokenized_field(
            log_entry_proto.message
        )
        # Handle dropped count first.
        if log_entry_proto.dropped:
            drop_reason = self.DROP_REASON_SOURCE_ENQUEUE_FAILURE
            if detokenized_message:
                drop_reason = detokenized_message.lower()
            return self._handle_log_drop_count(
                log_entry_proto.dropped, drop_reason
            )

        # Parse message and metadata, if any, encoded in a key-value format as
        # described in pw_log/log.proto LogEntry::message field.
        message_and_metadata = pw_log_tokenized.FormatStringWithMetadata(
            detokenized_message
        )
        module_name = self._decode_optionally_tokenized_field(
            log_entry_proto.module
        )
        if not module_name:
            module_name = message_and_metadata.module

        line_level_tuple = Log.unpack_line_level(log_entry_proto.line_level)
        file_and_line = self._decode_optionally_tokenized_field(
            log_entry_proto.file
        )
        if not file_and_line:
            # Set file name if found in the metadata.
            if message_and_metadata.file is not None:
                file_and_line = message_and_metadata.file
            else:
                file_and_line = ''

        # Add line number to filepath if needed.
        if line_level_tuple.line and ':' not in file_and_line:
            file_and_line += f':{line_level_tuple.line}'
        # Add extra log information avoiding duplicated data.
        metadata_fields = {
            k: v
            for k, v in message_and_metadata.fields.items()
            if k not in ['file', 'module', 'msg']
        }

        message = message_and_metadata.message
        if self.message_parser:
            message = self.message_parser(message)
        if self.timestamp_parser:
            timestamp = self.timestamp_parser(log_entry_proto.timestamp)
        else:
            timestamp = str(log_entry_proto.timestamp)
        log = Log(
            message=message,
            level=line_level_tuple.level,
            flags=log_entry_proto.flags,
            timestamp=timestamp,
            module_name=module_name,
            thread_name=self._decode_optionally_tokenized_field(
                log_entry_proto.thread
            ),
            source_name=self.source_name,
            file_and_line=file_and_line,
            metadata_fields=metadata_fields,
        )

        return log

    def _handle_log_drop_count(self, drop_count: int, reason: str) -> Log:
        log_word = 'logs' if drop_count > 1 else 'log'
        log = Log(
            message=f'Dropped {drop_count} {log_word} due to {reason}',
            level=logging.WARNING,
            source_name=self.source_name,
        )
        return log

    def _calculate_dropped_logs(
        self, log_entries_proto: log_pb2.LogEntries
    ) -> int:
        # Count log messages received that don't use the dropped field.
        messages_received = sum(
            1 if not log_proto.dropped else 0
            for log_proto in log_entries_proto.entries
        )
        dropped_log_count = (
            log_entries_proto.first_entry_sequence_id
            - self._expected_log_sequence_id
        )
        self._expected_log_sequence_id = (
            log_entries_proto.first_entry_sequence_id + messages_received
        )
        return dropped_log_count

    def _decode_optionally_tokenized_field(self, field: bytes) -> str:
        """Decodes tokenized field into a printable string.
        Args:
            field: Bytes.
        Returns:
            A printable string.
        """
        if self.detokenizer:
            return decode_optionally_tokenized(self.detokenizer, field)
        return field.decode('utf-8')
