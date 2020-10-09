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
    Invalid = 0
    Instantaneous = 1
    InstantaneousGroup = 2
    AsyncStart = 3
    AsyncStep = 4
    AsyncEnd = 5
    DurationStart = 6
    DurationEnd = 7
    DurationGroupStart = 8
    DurationGroupEnd = 9


class TraceEvent(NamedTuple):
    event_type: TraceType
    module: str
    label: str
    timestamp_us: int
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


def generate_trace_json(events: Iterable[TraceEvent]):
    """Generates a list of JSON lines from provided trace events."""
    json_lines = []
    for event in events:
        if event.module is None or event.timestamp_us is None or \
           event.event_type is None or event.label is None:
            _LOG.error("Invalid sample")
            continue

        line = {
            "pid": event.module,
            "name": (event.label),
            "ts": event.timestamp_us
        }
        if event.event_type == TraceType.DurationStart:
            line["ph"] = "B"
            line["tid"] = event.label
        elif event.event_type == TraceType.DurationEnd:
            line["ph"] = "E"
            line["tid"] = event.label
        elif event.event_type == TraceType.DurationGroupStart:
            line["ph"] = "B"
            line["tid"] = event.group
        elif event.event_type == TraceType.DurationGroupEnd:
            line["ph"] = "E"
            line["tid"] = event.group
        elif event.event_type == TraceType.Instantaneous:
            line["ph"] = "I"
            line["s"] = "p"
        elif event.event_type == TraceType.InstantaneousGroup:
            line["ph"] = "I"
            line["s"] = "t"
            line["tid"] = event.group
        elif event.event_type == TraceType.AsyncStart:
            line["ph"] = "b"
            line["scope"] = event.group
            line["tid"] = event.group
            line["cat"] = event.module
            line["id"] = event.trace_id
            line["args"] = {"id": line["id"]}
        elif event.event_type == TraceType.AsyncStep:
            line["ph"] = "n"
            line["scope"] = event.group
            line["tid"] = event.group
            line["cat"] = event.module
            line["id"] = event.trace_id
            line["args"] = {"id": line["id"]}
        elif event.event_type == TraceType.AsyncEnd:
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
                items = struct.unpack_from(
                    event.data_fmt[len("@pw_py_struct_fmt:"):], event.data)
                args = {}
                for i, item in enumerate(items):
                    args["data_" + str(i)] = item
                line["args"] = args
            else:
                line["args"] = {"data": event.data.hex()}

        # Encode as JSON
        json_lines.append(json.dumps(line))

    return json_lines
