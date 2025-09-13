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
import json
import logging
from typing import Any

from pw_tokenizer import detokenize

_LOG = logging.getLogger(__name__)


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
    timeout_s: float | None,
):
    """Fetches metrics using the legacy streaming RPC.

    For a more robust, transport-agnostic method, use get_all_metrics.
    """
    # Creates a defaultdict that can infinitely have other defaultdicts
    # without a specified type.
    metrics: defaultdict = _tree()
    if not detokenizer:
        _LOG.warning(
            'No metrics token database set; metric names will be tokens'
        )
    stream_response = rpcs.pw.metric.proto.MetricService.Get(
        pw_rpc_timeout_s=timeout_s
    )
    if not stream_response.status.ok():
        _LOG.error('Unexpected status %s', stream_response.status)
        return metrics
    for metric_response in stream_response.responses:
        for metric in metric_response.metrics:
            path_names = []
            for path in metric.token_path:
                if detokenizer:
                    lookup_result = detokenizer.lookup(path)
                    if lookup_result:
                        path_name = str(
                            detokenize.DetokenizedString(
                                path, lookup_result, b'', False
                            )
                        ).strip('"')
                    else:
                        path_name = f'${path:08x}'
                else:
                    path_name = f'${path:08x}'
                path_names.append(path_name)
            value = (
                metric.as_float
                if metric.HasField('as_float')
                else metric.as_int
            )
            # inserting path_names into metrics.
            _insert(metrics, path_names, value)
    # Converts default dict objects into standard dictionaries.
    return json.loads(json.dumps(metrics))


def get_all_metrics(
    rpcs: Any,
    detokenizer: detokenize.Detokenizer | None,
    timeout_s: float | None,
) -> dict:
    """Fetches all metrics using the paginated Walk RPC.

    This unary, client-driven RPC is suitable for asynchronous transports where
    the server cannot guarantee the transport is ready to send. Its paginated
    nature also makes it ideal for large metric sets that may exceed the
    transport MTU.
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
        request = rpcs.pw.metric.proto.WalkRequest(cursor=cursor)
        status, response = rpcs.pw.metric.proto.MetricService.Walk(
            request, pw_rpc_timeout_s=timeout_s
        )

        if not status.ok():
            _LOG.error('RPC failed: %s', status)
            break

        for metric in response.metrics:
            path_names = []
            # The token_path field contains the full, flattened path to the
            # metric, with the final element being the metric's own name.
            for path in metric.token_path:
                if detokenizer:
                    lookup_result = detokenizer.lookup(path)
                    if lookup_result:
                        path_name = str(
                            detokenize.DetokenizedString(
                                path, lookup_result, b'', False
                            )
                        ).strip('"')
                    else:
                        path_name = f'${path:08x}'
                else:
                    path_name = f'${path:08x}'
                path_names.append(path_name)

            value = (
                metric.as_float
                if metric.HasField('as_float')
                else metric.as_int
            )
            _insert(metrics, path_names, value)

        if response.done:
            break

        if not response.HasField('cursor'):
            _LOG.error('RPC response is not done but is missing a cursor')
            return {}
        cursor = response.cursor

    return json.loads(json.dumps(metrics))
