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
"""Defines a callback-based RPC ClientImpl to use with pw_rpc.Client.

callback_client.Impl supports invoking RPCs synchronously or asynchronously.
Asynchronous invocations use a callback.

Synchronous invocations look like a function call. When invoking a unary or
server streaming RPC, the request may be provided as a message object or as
keyword arguments for the message fields (but not both).

.. code-block:: python

  status, response = client.channel(1).rpcs.MyServer.MyUnary(some_field=123)

  # Calls with a server stream return a status and a list of responses.
  status, responses = rpcs.MyService.MyServerStreaming(Request(some_field=123))

Synchronous client and bidirectional streaming calls accept an iterable of
requests to send.

.. code-block:: python

  requests = [Request(a=1), Request(b=2)]
  status, response = rpcs.MyService.MyClientStreaming(requests)

  requests = [Request(a=1), Request(b=2)]
  status, responses = rpcs.MyService.MyBidirectionalStreaming(requests)

Synchronous invocations block until the RPC completes or times out. The calls
use the default timeout provided when the callback_client.Impl() is created, or
a timeout passed in through the pw_rpc_timeout_s argument. A timeout of None
means to wait indefinitely for a response.

Asynchronous invocations immediately return a call object. Callbacks may be
provided for the three types of RPC events:

  * on_next(call_object, response) - called for each response
  * on_completed(call_object, status) - called when the RPC completes
  * on_error(call_object, error) - called if the RPC terminates due to an error

If no callbacks are provided, the events are simply logged.

.. code-block:: python

  # Custom callbacks are called for each event.
  call = client.channel(1).call.MyService.MyServerStreaming.invoke(
      the_request, on_next_cb, on_completed_cb, on_error_cb):

  # A callback is called for the response. Other events are logged.
  call = client.channel(1).call.MyServer.MyUnary.invoke(
      the_request, lambda _, reply: process_this(reply))

For client and bidirectional streaming RPCs, requests are sent with the send
method. The finish_and_wait method finishes the client stream. It optionally
takes an iterable for responses to send before closing the stream.

.. code-block:: python

  # Start the call using callbacks.
  call = client.channel(1).call.MyServer.MyClientStream.invoke(on_error=err_cb)

  # Send a single client stream request.
  call.send(some_field=123)

  # Send the requests, close the stream, then wait for the RPC to complete.
  stream_responses = call.finish_and_wait([RequestType(some_field=123), ...])
"""

from pw_rpc.callback_client.call import (
    OptionalTimeout,
    UseDefault,
    UnaryResponse,
    StreamResponse,
    UnaryCall,
    ServerStreamingCall,
    ClientStreamingCall,
    BidirectionalStreamingCall,
    OnNextCallback,
    OnCompletedCallback,
    OnErrorCallback,
)
from pw_rpc.callback_client.errors import RpcError, RpcTimeout
from pw_rpc.callback_client.impl import Impl
