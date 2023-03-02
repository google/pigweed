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

Trace module which creates trace files from a list of trace events.

This is a work in progress, future work will look to add:
    - Config options to customize output.
    - A method of providing custom data formatters.
    - Perfetto support.
"""
from enum import Enum
import json
import logging
import struct
from typing import Iterable, NamedTuple

_LOG = logging.getLogger('pw_trace')


class TraceType(Enum):
    INVALID = 0
    INSTANTANEOUS = 1
    INSTANTANEOUS_GROUP = 2
    ASYNC_START = 3
    ASYNC_STEP = 4
    ASYNC_END = 5
    DURATION_START = 6
    DURATION_END = 7
    DURATION_GROUP_START = 8
    DURATION_GROUP_END = 9

    # TODO(hepler): Remove these aliases for the original style-incompliant
    #     names when users have migrated.
    DurationStart = 6  # pylint: disable=invalid-name
    DurationEnd = 7  # pylint: disable=invalid-name


class TraceEvent(NamedTuple):
    event_type: TraceType
    module: str
    label: str
    timestamp_us: float
    group: str = ""
    trace_id: int = 0
    flags: int = 0
    has_data: bool = False
    data_fmt: str = ""
    data: bytes = b''


def event_has_trace_id(event_type):
    return event_type in {
        "PW_TRACE_EVENT_TYPE_ASYNC_START",
        "PW_TRACE_EVENT_TYPE_ASYNC_STEP",
        "PW_TRACE_EVENT_TYPE_ASYNC_END",
    }


def decode_struct_fmt_args(event):
    """Decodes the trace's event data for struct-formatted data"""
    args = {}
    struct_fmt = event.data_fmt[len("@pw_py_struct_fmt:") :]
    try:
        # needed in case the buffer is larger than expected
        assert struct.calcsize(struct_fmt) == len(event.data)
        items = struct.unpack_from(struct_fmt, event.data)
        for i, item in enumerate(items):
            args["data_" + str(i)] = item
    except (AssertionError, struct.error):
        args["error"] = (
            f"Mismatched struct/data format {event.data_fmt} "
            f"expected data len {struct.calcsize(struct_fmt)} "
            f"data {event.data.hex()} "
            f"data len {len(event.data)}"
        )
    return args


def decode_map_fmt_args(event):
    """Decodes the trace's event data for map-formatted data"""
    args = {}
    fmt_list = event.data_fmt[len("@pw_py_map_fmt:") :].strip("{}").split(",")
    fmt_bytes = ''
    fields = []
    try:
        for pair in fmt_list:
            (field, value) = (s.strip() for s in pair.split(":"))
            fields.append(field)
            fmt_bytes += value
    except ValueError:
        args["error"] = f"Invalid map format {event.data_fmt}"
    else:
        try:
            # needed in case the buffer is larger than expected
            assert struct.calcsize(fmt_bytes) == len(event.data)
            items = struct.unpack_from(fmt_bytes, event.data)
            for i, item in enumerate(items):
                args[fields[i]] = item
        except (AssertionError, struct.error):
            args["error"] = (
                f"Mismatched struct/data format {event.data_fmt}"
                f" data {event.data.hex()}"
            )
    return args


def generate_trace_json(events: Iterable[TraceEvent]):
    """Generates a list of JSON lines from provided trace events."""
    json_lines = []
    for event in events:
        if (
            event.module is None
            or event.timestamp_us is None
            or event.event_type is None
            or event.label is None
        ):
            _LOG.error("Invalid sample")
            continue

        line = {
            "pid": event.module,
            "name": (event.label),
            "ts": event.timestamp_us,
        }
        if event.event_type == TraceType.DURATION_START:
            line["ph"] = "B"
            line["tid"] = event.label
        elif event.event_type == TraceType.DURATION_END:
            line["ph"] = "E"
            line["tid"] = event.label
        elif event.event_type == TraceType.DURATION_GROUP_START:
            line["ph"] = "B"
            line["tid"] = event.group
        elif event.event_type == TraceType.DURATION_GROUP_END:
            line["ph"] = "E"
            line["tid"] = event.group
        elif event.event_type == TraceType.INSTANTANEOUS:
            line["ph"] = "I"
            line["s"] = "p"
        elif event.event_type == TraceType.INSTANTANEOUS_GROUP:
            line["ph"] = "I"
            line["s"] = "t"
            line["tid"] = event.group
        elif event.event_type == TraceType.ASYNC_START:
            line["ph"] = "b"
            line["scope"] = event.group
            line["tid"] = event.group
            line["cat"] = event.module
            line["id"] = event.trace_id
            line["args"] = {"id": line["id"]}
        elif event.event_type == TraceType.ASYNC_STEP:
            line["ph"] = "n"
            line["scope"] = event.group
            line["tid"] = event.group
            line["cat"] = event.module
            line["id"] = event.trace_id
            line["args"] = {"id": line["id"]}
        elif event.event_type == TraceType.ASYNC_END:
            line["ph"] = "e"
            line["scope"] = event.group
            line["tid"] = event.group
            line["cat"] = event.module
            line["id"] = event.trace_id
            line["args"] = {"id": line["id"]}
        else:
            _LOG.error("Unknown event type, skipping")
            continue

        # Handle Data
        if event.has_data:
            if event.data_fmt == "@pw_arg_label":
                line["name"] = event.data.decode("utf-8")
            elif event.data_fmt == "@pw_arg_group":
                line["tid"] = event.data.decode("utf-8")
            elif event.data_fmt == "@pw_arg_counter":
                line["ph"] = "C"
                line["args"] = {
                    line["name"]: int.from_bytes(event.data, "little")
                }
            elif event.data_fmt.startswith("@pw_py_struct_fmt:"):
                line["args"] = decode_struct_fmt_args(event)
            elif event.data_fmt.startswith("@pw_py_map_fmt:"):
                line["args"] = decode_map_fmt_args(event)
            else:
                line["args"] = {"data": event.data.hex()}

        # Encode as JSON
        json_lines.append(json.dumps(line))

    return json_lines
