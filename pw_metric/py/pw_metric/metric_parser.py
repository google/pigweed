#!/usr/bin/env python3
# Copyright 2025 The Pigweed Authors
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
"""Tools to retrieve and parse metrics."""
from collections import defaultdict
import dataclasses
import json
import logging
from typing import Any, List, Union

from pw_tokenizer import detokenize
from pw_metric_proto import metric_service_pb2

_LOG = logging.getLogger(__name__)


@dataclasses.dataclass
class ParsedMetric:
    """Dataclass to hold a metric's detokenized path and value."""

    path_names: List[str] = dataclasses.field(default_factory=list)
    value: Union[float, int] = 0


def parse_metric(
    metric: metric_service_pb2.Metric,
    detokenizer: detokenize.Detokenizer | None,
) -> ParsedMetric:
    """Parses a single Metric proto, detokenizing its path."""
    parsed_metric = ParsedMetric()
    for path_token in metric.token_path:
        path_name = f'${path_token:08x}'  # Default to token if not found
        if detokenizer:
            lookup_result = detokenizer.lookup(path_token)
            if lookup_result:
                # Manually construct a DetokenizedString and strip quotes.
                path_name = str(
                    detokenize.DetokenizedString(
                        path_token, lookup_result, b'', False
                    )
                ).strip('"')
        parsed_metric.path_names.append(path_name)

    value_type = metric.WhichOneof('value')
    if value_type == 'as_float':
        parsed_metric.value = metric.as_float
    elif value_type == 'as_int':
        parsed_metric.value = metric.as_int
    return parsed_metric


def _tree():
    """Creates a key based on given input."""
    return defaultdict(_tree)


def _insert(metrics, path_names, value):
    """Inserts any value in a leaf of the dictionary."""
    for index, path_name in enumerate(path_names):
        if index < len(path_names) - 1:
            metrics = metrics[path_name]
        elif path_name in metrics:
            # the value in this position isn't a float or int,
            # then collision occurs, throw an error.
            raise ValueError('Variable already exists: {p}'.format(p=path_name))
        else:
            metrics[path_name] = value


def parse_metrics(
    rpcs: Any,
    detokenizer: detokenize.Detokenizer | None,
    timeout_s: float | None = 5,
) -> dict:
    """Retrieves all metrics using the legacy server-streaming Get RPC.

    NOTE: This function uses the legacy server-streaming MetricService.Get RPC.
    This method is suitable for transports that can support server-side
    streaming, but may be less robust on asynchronous or MTU-limited
    transports.

    The `get_all_metrics()` function (which uses the unary `Walk` RPC) is the
    generally preferred method for large metric sets that may exceed the
    transport MTU.
    """
    # Creates a defaultdict that can infinitely have other defaultdicts
    # without a specified type.
    metrics: defaultdict = _tree()

    if not detokenizer:
        _LOG.warning(
            'No metrics token database set; metric names will be tokens'
        )
    stream_response = rpcs.pw.metric.proto.MetricService.Get(
        metric_service_pb2.MetricRequest(), pw_rpc_timeout_s=timeout_s
    )
    if not stream_response.status.ok():
        _LOG.error('RPC failed: %s', stream_response.status)
        return {}
    # Iterate over each payload received in the stream
    for metric_response in stream_response.responses:
        for metric in metric_response.metrics:
            parsed = parse_metric(metric, detokenizer)
            # inserting path_names into metrics.
            _insert(metrics, parsed.path_names, parsed.value)
    # Converts default dict objects into standard dictionaries.
    return json.loads(json.dumps(metrics))


def get_all_metrics(
    rpcs: Any,
    detokenizer: detokenize.Detokenizer | None,
    timeout_s: float | None = 5,
) -> dict:
    """Retrieves all metrics and inserts them into a dictionary.

    This function uses the unary, client-driven MetricService.Walk RPC.
    This RPC is suitable for asynchronous transports where the server cannot
    guarantee transport readiness. Its paginated nature also makes it ideal for
    large metric sets that may exceed the transport MTU.
    """
    metrics: defaultdict = _tree()
    if not detokenizer:
        _LOG.warning(
            'No metrics token database set. Metric names will be shown as '
            'tokens'
        )

    cursor = 0
    # Repeatedly call the Walk RPC, passing the cursor from the previous
    # response until the server indicates the walk is complete.
    while True:
        # The python generated proto uses WalkRequest instead of Message
        request = metric_service_pb2.WalkRequest(cursor=cursor)
        status, response = rpcs.pw.metric.proto.MetricService.Walk(
            request, pw_rpc_timeout_s=timeout_s
        )

        if not status.ok():
            _LOG.error('RPC failed: %s', status)
            break

        if response:
            for metric in response.metrics:
                parsed = parse_metric(metric, detokenizer)
                _insert(metrics, parsed.path_names, parsed.value)

            if response.done:
                break

            if not response.HasField('cursor'):
                _LOG.error('RPC response is not done but is missing a cursor')
                return {}
            cursor = response.cursor
        else:
            # Handle cases where the call was successful but the response is
            #  empty
            if status.ok():
                break

            _LOG.error('RPC call failed and returned no response payload')
            break

    return json.loads(json.dumps(metrics))
