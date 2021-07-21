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

import enum
import inspect
import logging
import queue
import textwrap
from typing import (Any, Callable, Iterable, Iterator, NamedTuple, Union,
                    Optional, Sequence, Type, TypeVar)

from pw_protobuf_compiler.python_protos import proto_repr
from pw_status import Status
from google.protobuf.message import Message

from pw_rpc import client, descriptors
from pw_rpc.client import PendingRpc, PendingRpcs
from pw_rpc.descriptors import Channel, Method, Service

_LOG = logging.getLogger(__package__)


class UseDefault(enum.Enum):
    """Marker for args that should use a default value, when None is valid."""
    VALUE = 0


_CallType = TypeVar('_CallType', 'UnaryCall', 'ServerStreamingCall',
                    'ClientStreamingCall', 'BidirectionalStreamingCall')

OnNextCallback = Callable[[_CallType, Any], Any]
OnCompletedCallback = Callable[[_CallType, Any], Any]
OnErrorCallback = Callable[[_CallType, Any], Any]

OptionalTimeout = Union[UseDefault, float, None]


class _MethodClient:
    """A method that can be invoked for a particular channel."""
    def __init__(self, client_impl: 'Impl', rpcs: PendingRpcs,
                 channel: Channel, method: Method,
                 default_timeout_s: Optional[float]):
        self._impl = client_impl
        self._rpcs = rpcs
        self._rpc = PendingRpc(channel, method.service, method)
        self.default_timeout_s: Optional[float] = default_timeout_s

    @property
    def channel(self) -> Channel:
        return self._rpc.channel

    @property
    def method(self) -> Method:
        return self._rpc.method

    @property
    def service(self) -> Service:
        return self._rpc.service

    def __repr__(self) -> str:
        return self.help()

    def help(self) -> str:
        """Returns a help message about this RPC."""
        function_call = self.method.full_name + '('

        docstring = inspect.getdoc(self.__call__)  # type: ignore[operator] # pylint: disable=no-member
        assert docstring is not None

        annotation = inspect.Signature.from_callable(self).return_annotation  # type: ignore[arg-type] # pylint: disable=line-too-long
        if isinstance(annotation, type):
            annotation = annotation.__name__

        arg_sep = f',\n{" " * len(function_call)}'
        return (
            f'{function_call}'
            f'{arg_sep.join(descriptors.field_help(self.method.request_type))})'
            f'\n\n{textwrap.indent(docstring, "  ")}\n\n'
            f'  Returns {annotation}.')

    def _start_call(self, call_type: Type[_CallType],
                    request: Optional[Message], timeout_s: OptionalTimeout,
                    on_next: Optional[OnNextCallback],
                    on_completed: Optional[OnCompletedCallback],
                    on_error: Optional[OnErrorCallback]) -> _CallType:
        """Creates the Call object and invokes the RPC using it."""
        if timeout_s is UseDefault.VALUE:
            timeout_s = self.default_timeout_s

        call = call_type(self._rpcs, self._rpc, timeout_s, on_next,
                         on_completed, on_error)
        call._invoke(request)  # pylint: disable=protected-access
        return call

    def _client_streaming_call_type(self,
                                    base: Type[_CallType]) -> Type[_CallType]:
        """Creates a client or bidirectional stream call type.

        Applies the signature from the request protobuf to the send method.
        """
        def send(self,
                 _rpc_request_proto: Message = None,
                 **request_fields) -> None:
            ClientStreamingCall.send(self, _rpc_request_proto,
                                     **request_fields)

        _apply_protobuf_signature(self.method, send)

        return type(f'{self.method.name}_{base.__name__}', (base, ),
                    dict(send=send))


class RpcTimeout(Exception):
    def __init__(self, rpc: PendingRpc, timeout: Optional[float]):
        super().__init__(
            f'No response received for {rpc.method} after {timeout} s')
        self.rpc = rpc
        self.timeout = timeout


class RpcError(Exception):
    def __init__(self, rpc: PendingRpc, status: Status):
        """Raised when there is an RPC-layer error."""
        if status is Status.NOT_FOUND:
            msg = ': the RPC server does not support this RPC'
        elif status is Status.DATA_LOSS:
            msg = ': an error occurred while decoding the RPC payload'
        else:
            msg = ''

        super().__init__(f'{rpc.method} failed with error {status}{msg}')
        self.rpc = rpc
        self.status = status


class UnaryResponse(NamedTuple):
    """Result from a unary or client streaming RPC: status and response."""
    status: Status
    response: Any

    def __repr__(self) -> str:
        return f'({self.status}, {proto_repr(self.response)})'


class StreamResponse(NamedTuple):
    """Results from a server or bidirectional streaming RPC."""
    status: Status
    responses: Sequence[Any]

    def __repr__(self) -> str:
        return (f'({self.status}, '
                f'[{", ".join(proto_repr(r) for r in self.responses)}])')


class _InternalCallbacks(NamedTuple):
    """Callbacks used with the low-level pw_rpc client interface."""
    on_next: Callable[[Message], None]
    on_completed: Callable[[Status], None]
    on_error: Callable[[Status], None]


class _Call:
    """Represents an in-progress or completed RPC call."""
    def __init__(self, rpcs: PendingRpcs, rpc: PendingRpc,
                 default_timeout_s: Optional[float],
                 on_next: Optional[OnNextCallback],
                 on_completed: Optional[OnCompletedCallback],
                 on_error: Optional[OnErrorCallback]) -> None:
        self._rpcs = rpcs
        self._rpc = rpc
        self.default_timeout_s = default_timeout_s

        self.status: Optional[Status] = None
        self.error: Optional[RpcError] = None
        self._responses: list = []
        self._response_queue: queue.SimpleQueue = queue.SimpleQueue()

        self.on_next = on_next or _Call._default_response
        self.on_completed = on_completed or _Call._default_completion
        self.on_error = on_error or _Call._default_error

    def _invoke(self, request: Optional[Message]) -> None:
        """Calls the RPC. This must be called immediately after __init__."""
        self._rpcs.send_request(self._rpc,
                                request,
                                _InternalCallbacks(self._handle_response,
                                                   self._handle_completion,
                                                   self._handle_error),
                                override_pending=True)

    def _default_response(self, response: Message) -> None:
        _LOG.debug('%s received response: %s', self._rpc, response)

    def _default_completion(self, status: Status) -> None:
        _LOG.info('%s completed: %s', self._rpc, status)

    def _default_error(self, error: Status) -> None:
        _LOG.warning('%s termianted due to an error: %s', self._rpc, error)

    @property
    def method(self) -> Method:
        return self._rpc.method

    def completed(self) -> bool:
        """True if the RPC call has completed, successfully or from an error."""
        return self.status is not None or self.error is not None

    def _send_client_stream(self, request_proto, request_fields: dict) -> None:
        """Sends a client to the server in the client stream."""
        if self.error:
            raise self.error

        if self.status is not None:
            raise RpcError(self._rpc, Status.FAILED_PRECONDITION)

        self._rpcs.send_client_stream(
            self._rpc, self.method.get_request(request_proto, request_fields))

    def _finish_client_stream(self, requests: Iterable[Message]) -> None:
        for request in requests:
            self._send_client_stream(request, {})

        if not self.completed():
            self._rpcs.send_client_stream_end(self._rpc)

    def _unary_wait(self, timeout_s: OptionalTimeout) -> UnaryResponse:
        """Waits until the RPC has completed."""
        for _ in self._get_responses(block=True, timeout_s=timeout_s):
            pass

        assert self.status is not None and self._responses
        return UnaryResponse(self.status, self._responses[-1])

    def _stream_wait(self, timeout_s: OptionalTimeout) -> StreamResponse:
        """Waits until the RPC has completed."""
        for _ in self._get_responses(block=True, timeout_s=timeout_s):
            pass

        assert self.status is not None
        return StreamResponse(self.status, self._responses)

    def _get_responses(self, *, block: bool,
                       timeout_s: OptionalTimeout) -> Iterator:
        """Returns an iterator of stream responses.

        Args:
          timeout_s: timeout in seconds; None blocks indefinitely
        """
        if self.error:
            raise self.error

        if self.completed():
            return

        if timeout_s is UseDefault.VALUE:
            timeout_s = self.default_timeout_s

        try:
            while True:
                response = self._response_queue.get(block, timeout_s)

                if isinstance(response, Exception):
                    raise response

                if isinstance(response, Status):
                    return

                yield response
        except queue.Empty:
            self.cancel()
            raise RpcTimeout(self._rpc, timeout_s)
        except:
            self.cancel()
            raise

    def cancel(self) -> bool:
        """Cancels the RPC; returns whether the RPC was active."""
        if self.completed():
            return False

        self.error = RpcError(self._rpc, Status.CANCELLED)
        return self._rpcs.send_cancel(self._rpc)

    def _handle_response(self, response: Any) -> None:
        # TODO(frolv): These lists could grow very large for persistent
        # streaming RPCs such as logs. The size should be limited.
        self._responses.append(response)
        self._response_queue.put(response)

        self.on_next(self, response)

    def _handle_completion(self, status: Status) -> None:
        self.status = status
        self._response_queue.put(status)

        self.on_completed(self, status)

    def _handle_error(self, error: Status) -> None:
        self.error = RpcError(self._rpc, error)
        self._response_queue.put(self.error)

        self.on_error(self, error)

    def __enter__(self) -> '_Call':
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.cancel()

    def __repr__(self) -> str:
        return f'{type(self).__name__}({self.method})'


def _function_docstring(method: Method) -> str:
    return f'''\
Invokes the {method.full_name} {method.type.sentence_name()} RPC.

This function accepts either the request protobuf fields as keyword arguments or
a request protobuf as a positional argument.
'''


def _update_call_method(method: Method, function: Callable) -> None:
    """Updates the name, docstring, and parameters to match a method."""
    function.__name__ = method.full_name
    function.__doc__ = _function_docstring(method)
    _apply_protobuf_signature(method, function)


def _apply_protobuf_signature(method: Method, function: Callable) -> None:
    """Update a function signature to accept proto arguments.

    In order to have good tab completion and help messages, update the function
    signature to accept only keyword arguments for the proto message fields.
    This doesn't actually change the function signature -- it just updates how
    it appears when inspected.
    """
    sig = inspect.signature(function)

    params = [next(iter(sig.parameters.values()))]  # Get the "self" parameter
    params += method.request_parameters()
    params.append(
        inspect.Parameter('pw_rpc_timeout_s', inspect.Parameter.KEYWORD_ONLY))

    function.__signature__ = sig.replace(  # type: ignore[attr-defined]
        parameters=params)


class UnaryCall(_Call):
    """Tracks the state of a unary RPC call."""
    @property
    def response(self) -> Any:
        return self._responses[-1] if self._responses else None

    def wait(self,
             timeout_s: OptionalTimeout = UseDefault.VALUE) -> UnaryResponse:
        return self._unary_wait(timeout_s)


class ServerStreamingCall(_Call):
    """Tracks the state of a server streaming RPC call."""
    @property
    def responses(self) -> Sequence:
        return self._responses

    def wait(self,
             timeout_s: OptionalTimeout = UseDefault.VALUE) -> StreamResponse:
        return self._stream_wait(timeout_s)

    def get_responses(
            self,
            *,
            block: bool = True,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> Iterator:
        return self._get_responses(block=block, timeout_s=timeout_s)

    def __iter__(self) -> Iterator:
        return self.get_responses()


class ClientStreamingCall(_Call):
    """Tracks the state of a client streaming RPC call."""
    @property
    def response(self) -> Any:
        return self._responses[-1] if self._responses else None

    # TODO(hepler): Use / to mark the first arg as positional-only when
    #     Python 3.7 support is no longer required.
    def send(self,
             _rpc_request_proto: Message = None,
             **request_fields) -> None:
        """Sends client stream request to the server."""
        self._send_client_stream(_rpc_request_proto, request_fields)

    def finish_and_wait(
            self,
            requests: Iterable[Message] = (),
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> UnaryResponse:
        """Ends the client stream and waits for the RPC to complete."""
        self._finish_client_stream(requests)
        return self._unary_wait(timeout_s)

    def get_responses(
            self,
            *,
            block: bool = True,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> Iterator:
        return self._get_responses(block=block, timeout_s=timeout_s)

    def __iter__(self) -> Iterator:
        return self.get_responses()


class BidirectionalStreamingCall(_Call):
    """Tracks the state of a bidirectional streaming RPC call."""
    @property
    def responses(self) -> Sequence:
        return self._responses

    # TODO(hepler): Use / to mark the first arg as positional-only when
    #     Python 3.7 support is no longer required.
    def send(self,
             _rpc_request_proto: Message = None,
             **request_fields) -> None:
        """Sends a message to the server in the client stream."""
        self._send_client_stream(_rpc_request_proto, request_fields)

    def finish_and_wait(
            self,
            requests: Iterable[Message] = (),
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> StreamResponse:
        """Ends the client stream and waits for the RPC to complete."""
        self._finish_client_stream(requests)
        return self._stream_wait(timeout_s)

    def get_responses(
            self,
            *,
            block: bool = True,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> Iterator:
        return self._get_responses(block=block, timeout_s=timeout_s)

    def __iter__(self) -> Iterator:
        return self.get_responses()


class _UnaryMethodClient(_MethodClient):
    def invoke(self,
               request: Message = None,
               on_next: OnNextCallback = None,
               on_completed: OnCompletedCallback = None,
               on_error: OnErrorCallback = None,
               *,
               timeout_s: OptionalTimeout = UseDefault.VALUE) -> UnaryCall:
        """Invokes the unary RPC and returns a call object."""
        return self._start_call(UnaryCall, request, timeout_s, on_next,
                                on_completed, on_error)


class _ServerStreamingMethodClient(_MethodClient):
    def invoke(
            self,
            request: Message = None,
            on_next: OnNextCallback = None,
            on_completed: OnCompletedCallback = None,
            on_error: OnErrorCallback = None,
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE
    ) -> ServerStreamingCall:
        """Invokes the server streaming RPC and returns a call object."""
        return self._start_call(ServerStreamingCall, request, timeout_s,
                                on_next, on_completed, on_error)


class _ClientStreamingMethodClient(_MethodClient):
    def invoke(
            self,
            on_next: OnNextCallback = None,
            on_completed: OnCompletedCallback = None,
            on_error: OnErrorCallback = None,
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE
    ) -> ClientStreamingCall:
        """Invokes the client streaming RPC and returns a call object"""
        return self._start_call(
            self._client_streaming_call_type(ClientStreamingCall), None,
            timeout_s, on_next, on_completed, on_error)

    def __call__(
            self,
            requests: Iterable[Message] = (),
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> UnaryResponse:
        return self.invoke().finish_and_wait(requests, timeout_s=timeout_s)


class _BidirectionalStreamingMethodClient(_MethodClient):
    """Invokes the bidirectional streaming RPC and returns a call object."""
    def invoke(
        self,
        on_next: OnNextCallback = None,
        on_completed: OnCompletedCallback = None,
        on_error: OnErrorCallback = None,
        *,
        timeout_s: OptionalTimeout = UseDefault.VALUE
    ) -> BidirectionalStreamingCall:
        return self._start_call(
            self._client_streaming_call_type(BidirectionalStreamingCall), None,
            timeout_s, on_next, on_completed, on_error)

    def __call__(
            self,
            requests: Iterable[Message] = (),
            *,
            timeout_s: OptionalTimeout = UseDefault.VALUE) -> StreamResponse:
        return self.invoke().finish_and_wait(requests, timeout_s=timeout_s)


def _method_client_docstring(method: Method) -> str:
    return f'''\
Class that invokes the {method.full_name} {method.type.sentence_name()} RPC.

Calling this directly invokes the RPC synchronously. The RPC can be invoked
asynchronously using the invoke method.
'''


class Impl(client.ClientImpl):
    """Callback-based ClientImpl."""
    def __init__(self,
                 default_unary_timeout_s: float = None,
                 default_stream_timeout_s: float = None):
        super().__init__()
        self._default_unary_timeout_s = default_unary_timeout_s
        self._default_stream_timeout_s = default_stream_timeout_s

    @property
    def default_unary_timeout_s(self) -> Optional[float]:
        return self._default_unary_timeout_s

    @property
    def default_stream_timeout_s(self) -> Optional[float]:
        return self._default_stream_timeout_s

    def method_client(self, channel: Channel, method: Method) -> _MethodClient:
        """Returns an object that invokes a method using the given chanel."""

        if method.type is Method.Type.UNARY:
            return self._create_unary_method_client(
                channel, method, self.default_unary_timeout_s)

        if method.type is Method.Type.SERVER_STREAMING:
            return self._create_server_streaming_method_client(
                channel, method, self.default_stream_timeout_s)

        if method.type is Method.Type.CLIENT_STREAMING:
            return self._create_method_client(_ClientStreamingMethodClient,
                                              channel, method,
                                              self.default_unary_timeout_s)

        if method.type is Method.Type.BIDIRECTIONAL_STREAMING:
            return self._create_method_client(
                _BidirectionalStreamingMethodClient, channel, method,
                self.default_stream_timeout_s)

        raise AssertionError(f'Unknown method type {method.type}')

    def _create_method_client(self, base: type, channel: Channel,
                              method: Method,
                              default_timeout_s: Optional[float], **fields):
        """Creates a _MethodClient derived class customized for the method."""
        method_client_type = type(
            f'{method.name}{base.__name__}', (base, ),
            dict(__doc__=_method_client_docstring(method), **fields))
        return method_client_type(self, self.rpcs, channel, method,
                                  default_timeout_s)

    def _create_unary_method_client(
            self, channel: Channel, method: Method,
            default_timeout_s: Optional[float]) -> _UnaryMethodClient:
        """Creates a _UnaryMethodClient with a customized __call__ method."""

        # TODO(hepler): Use / to mark the first arg as positional-only when
        #     Python 3.7 support is no longer required.
        def call(self: _UnaryMethodClient,
                 _rpc_request_proto: Message = None,
                 *,
                 pw_rpc_timeout_s: OptionalTimeout = UseDefault.VALUE,
                 **request_fields) -> UnaryResponse:
            return self.invoke(
                self.method.get_request(_rpc_request_proto,
                                        request_fields)).wait(pw_rpc_timeout_s)

        _update_call_method(method, call)
        return self._create_method_client(_UnaryMethodClient,
                                          channel,
                                          method,
                                          default_timeout_s,
                                          __call__=call)

    def _create_server_streaming_method_client(
            self, channel: Channel, method: Method,
            default_timeout_s: Optional[float]
    ) -> _ServerStreamingMethodClient:
        """Creates _ServerStreamingMethodClient with custom __call__ method."""

        # TODO(hepler): Use / to mark the first arg as positional-only when
        #     Python 3.7 support is no longer required.
        def call(self: _ServerStreamingMethodClient,
                 _rpc_request_proto: Message = None,
                 *,
                 pw_rpc_timeout_s: OptionalTimeout = UseDefault.VALUE,
                 **request_fields) -> StreamResponse:
            return self.invoke(
                self.method.get_request(_rpc_request_proto,
                                        request_fields)).wait(pw_rpc_timeout_s)

        _update_call_method(method, call)
        return self._create_method_client(_ServerStreamingMethodClient,
                                          channel,
                                          method,
                                          default_timeout_s,
                                          __call__=call)

    def handle_response(self,
                        rpc: PendingRpc,
                        context: _InternalCallbacks,
                        payload,
                        *,
                        args: tuple = (),
                        kwargs: dict = None) -> None:
        """Invokes the callback associated with this RPC."""
        assert not args and not kwargs, 'Forwarding args & kwargs not supported'
        # Catch and log any exceptions from the user-provided callback so that
        # exceptions don't terminate the thread that is handling RPC packets.
        try:
            context.on_next(payload)
        except:  # pylint: disable=bare-except
            self.rpcs.send_cancel(rpc)
            _LOG.exception('Response callback %s for %s raised exception',
                           context.on_next, rpc)

    def handle_completion(self,
                          rpc: PendingRpc,
                          context: _InternalCallbacks,
                          status: Status,
                          *,
                          args: tuple = (),
                          kwargs: dict = None):
        assert not args and not kwargs, 'Forwarding args & kwargs not supported'
        try:
            context.on_completed(status)
        except:  # pylint: disable=bare-except
            _LOG.exception('Completion callback %s for %s raised exception',
                           context.on_completed, rpc)

    def handle_error(self,
                     rpc: PendingRpc,
                     context: _InternalCallbacks,
                     status: Status,
                     *,
                     args: tuple = (),
                     kwargs: dict = None) -> None:
        assert not args and not kwargs, 'Forwarding args & kwargs not supported'
        try:
            context.on_error(status)
        except:  # pylint: disable=bare-except
            _LOG.exception('Error callback %s for %s raised exception',
                           context.on_error, rpc)
