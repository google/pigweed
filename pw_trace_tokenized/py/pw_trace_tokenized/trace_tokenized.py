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
r"""
Generates json trace files viewable using chrome://tracing from binary
trace files.

Example usage:
python pw_trace_tokenized/py/trace_tokenized.py -i trace.bin -o trace.json
./out/host_clang_debug/obj/pw_trace_tokenized/bin/trace_tokenized_example_basic
"""
from enum import IntEnum
import argparse
import logging
import struct
import sys
from pw_tokenizer import database, tokens
from pw_trace import trace

_LOG = logging.getLogger('pw_trace_tokenizer')


def varint_decode(encoded):
    # Taken from pw_tokenizer.decode._decode_signed_integer
    count = 0
    result = 0
    shift = 0
    for byte in encoded:
        count += 1
        result |= (byte & 0x7f) << shift
        if not byte & 0x80:
            return result, count

        shift += 7
        if shift >= 64:
            break  # Error
    return None


# Token string: "event_type|flag|module|group|label|<optional data_fmt>"
class TokenIdx(IntEnum):
    EventType = 0
    Flag = 1
    Module = 2
    Group = 3
    Label = 4
    data_fmt = 5  # optional


def get_trace_type(type_str):
    if type_str == "PW_TRACE_EVENT_TYPE_INSTANT":
        return trace.TraceType.Instantaneous
    if type_str == "PW_TRACE_EVENT_TYPE_INSTANT_GROUP":
        return trace.TraceType.InstantaneousGroup
    if type_str == "PW_TRACE_EVENT_TYPE_ASYNC_START":
        return trace.TraceType.AsyncStart
    if type_str == "PW_TRACE_EVENT_TYPE_ASYNC_STEP":
        return trace.TraceType.AsyncStep
    if type_str == "PW_TRACE_EVENT_TYPE_ASYNC_END":
        return trace.TraceType.AsyncEnd
    if type_str == "PW_TRACE_EVENT_TYPE_DURATION_START":
        return trace.TraceType.DurationStart
    if type_str == "PW_TRACE_EVENT_TYPE_DURATION_END":
        return trace.TraceType.DurationEnd
    if type_str == "PW_TRACE_EVENT_TYPE_DURATION_GROUP_START":
        return trace.TraceType.DurationGroupStart
    if type_str == "PW_TRACE_EVENT_TYPE_DURATION_GROUP_END":
        return trace.TraceType.DurationGroupEnd
    return trace.TraceType.Invalid


def has_trace_id(token_string):
    token_values = token_string.split("|")
    return trace.event_has_trace_id(token_values[TokenIdx.EventType])


def has_data(token_string):
    token_values = token_string.split("|")
    return len(token_values) > TokenIdx.data_fmt


def create_trace_event(token_string, timestamp_us, trace_id, data):
    token_values = token_string.split("|")
    return trace.TraceEvent(event_type=get_trace_type(
        token_values[TokenIdx.EventType]),
                            module=token_values[TokenIdx.Module],
                            label=token_values[TokenIdx.Label],
                            timestamp_us=timestamp_us,
                            group=token_values[TokenIdx.Group],
                            trace_id=trace_id,
                            flags=token_values[TokenIdx.Flag],
                            has_data=has_data(token_string),
                            data_fmt=(token_values[TokenIdx.data_fmt]
                                      if has_data(token_string) else ""),
                            data=data if has_data(token_string) else b'')


def parse_trace_event(buffer, db, last_time, ticks_per_second=1000):
    us_per_tick = 1000000 / ticks_per_second
    idx = 0
    # Read token
    token = struct.unpack('I', buffer[idx:idx + 4])[0]
    idx += 4

    # Decode token
    if len(db.token_to_entries[token]) == 0:
        _LOG.error("token not found: %08x", token)
    token_string = str(db.token_to_entries[token][0])

    # Read time
    time_delta, time_bytes = varint_decode(buffer[idx:])
    timestamp_us = last_time + us_per_tick * time_delta
    idx += time_bytes

    # Trace ID
    trace_id = None
    if has_trace_id(token_string) and idx < len(buffer):
        trace_id, trace_id_bytes = varint_decode(buffer[idx:])
        idx += trace_id_bytes

    # Data
    data = None
    if has_data(token_string) and idx < len(buffer):
        data = buffer[idx:]

    # Create trace event
    return create_trace_event(token_string, timestamp_us, trace_id, data)


def get_trace_events_from_file(databases, input_file_name):
    """Handles the decoding traces."""

    db = tokens.Database.merged(*databases)
    last_timestamp = 0
    events = []
    with open(input_file_name, "rb") as input_file:
        bytes_read = input_file.read()
        idx = 0

        while idx + 1 < len(bytes_read):
            # Read size
            size = int(bytes_read[idx])
            if idx + size > len(bytes_read):
                _LOG.error("incomplete file")
                break

            event = parse_trace_event(bytes_read[idx + 1:idx + 1 + size], db,
                                      last_timestamp)
            last_timestamp = event.timestamp_us
            events.append(event)
            idx = idx + size + 1
    return events


def _parse_args():
    """Parse and return command line arguments."""

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        'databases',
        nargs='+',
        action=database.LoadTokenDatabases,
        help='Databases (ELF, binary, or CSV) to use to lookup tokens.')
    parser.add_argument(
        '-i',
        '--input',
        dest='input_file',
        help='The binary trace input file, generated using trace_to_file.h.')
    parser.add_argument('-o',
                        '--output',
                        dest='output_file',
                        help=('The json file to which to write the output.'))

    return parser.parse_args()


def _main(args):
    events = get_trace_events_from_file(args.databases, args.input_file)
    json_lines = trace.generate_trace_json(events)

    with open(args.output_file, 'w') as output_file:
        for line in json_lines:
            output_file.write("%s,\n" % line)


if __name__ == '__main__':
    if sys.version_info[0] < 3:
        sys.exit('ERROR: The detokenizer command line tools require Python 3.')
    _main(_parse_args())
