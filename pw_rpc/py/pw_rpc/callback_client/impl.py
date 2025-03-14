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
"""The callback-based pw_rpc client implementation."""

from __future__ import annotations

import inspect
import logging
import textwrap
from typing import Any, Callable, Iterable, Type

from dataclasses import dataclass
from pw_status import Status
from google.protobuf.message import Message

from pw_rpc import client, descriptors
from pw_rpc.client import PendingRpc, PendingRpcs
from pw_rpc.descriptors import Channel, Method, Service

from pw_rpc.callback_client.call import (
    UseDefault,
    OptionalTimeout,
    CallTypeT,
    UnaryResponse,
    StreamResponse,
    Call,
    UnaryCall,
    ServerStreamingCall,
    ClientStreamingCall,
    BidirectionalStreamingCall,
    OnNextCallback,
    OnCompletedCallback,
    OnErrorCallback,
)

_LOG = logging.getLogger(__package__)


DEFAULT_MAX_STREAM_RESPONSES = 2**14


@dataclass(eq=True, frozen=True)
class CallInfo:
    method: Method

    @property
    def service(self) -> Service:
        return self.method.service


class _MethodClient:
    """A method that can be invoked for a particular channel."""

    def __init__(
        self,
        client_impl: Impl,
        rpcs: PendingRpcs,
        channel: Channel,
        method: Method,
        default_timeout_s: float | None,
    ) -> None:
        self._impl = client_impl
        self._rpcs = rpcs
        self._channel = channel
        self._method = method
        self.default_timeout_s: float | None = default_timeout_s

    @property
    def channel(self) -> Channel:
        return self._channel

    @property
    def method(self) -> Method:
        return self._method

    @property
    def service(self) -> Service:
        return self._method.service

    @property
    def request(self) -> type:
        """Returns the request proto class."""
        return self.method.request_type

    @property
    def response(self) -> type:
        """Returns the response proto class."""
        return self.method.response_type

    def __repr__(self) -> str:
        return self.help()

    def help(self) -> str:
        """Returns a help message about this RPC."""
        function_call = self.method.full_name + '('

        docstring = inspect.getdoc(self.__call__)  # type: ignore[operator] # pylint: disable=no-member
        assert docstring is not None

        annotation = inspect.signature(self).return_annotation  # type: ignore[arg-type] # pylint: disable=line-too-long
        if isinstance(annotation, type):
            annotation = annotation.__name__

        arg_sep = f',\n{" " * len(function_call)}'
        return (
            f'{function_call}'
            f'{arg_sep.join(descriptors.field_help(self.method.request_type))})'
            f'\n\n{textwrap.indent(docstring, "  ")}\n\n'
            f'  Returns {annotation}.'
        )

    def _start_call(
        self,
        call_type: Type[CallTypeT],
        request: Message | None,
        timeout_s: OptionalTimeout,
        on_next: OnNextCallback | None,
        on_completed: OnCompletedCallback | None,
        on_error: OnErrorCallback | None,
        max_responses: int,
    ) -> CallTypeT:
        """Creates the Call object and invokes the RPC using it."""
        if timeout_s is UseDefault.VALUE:
            timeout_s = self.default_timeout_s

        if self._impl.on_call_hook:
            self._impl.on_call_hook(CallInfo(self._method))

        rpc = PendingRpc(
            self._channel,
            self.service,
            self.method,
            self._rpcs.allocate_call_id(),
        )
        call = call_type(
            self._rpcs,
            rpc,
            timeout_s,
            on_next,
            on_completed,
            on_error,
            max_responses,
        )
        call._invoke(request)  # pylint: disable=protected-access
        return call

    def _open_call(
        self,
        call_type: Type[CallTypeT],
        on_next: OnNextCallback | None,
        on_completed: OnCompletedCallback | None,
        on_error: OnErrorCallback | None,
        max_responses: int,
    ) -> CallTypeT:
        """Creates a Call object with the open call ID."""
        rpc = PendingRpc(
            self._channel,
            self.service,
            self.method,
            client.OPEN_CALL_ID,
        )
        call = call_type(
            self._rpcs,
            rpc,
            None,
            on_next,
            on_completed,
            on_error,
            max_responses,
        )
        call._open()  # pylint: disable=protected-access
        return call

    def _client_streaming_call_type(
        self, base: Type[CallTypeT]
    ) -> Type[CallTypeT]:
        """Creates a client or bidirectional stream call type.

        Applies the signature from the request protobuf to the send method.
        """

        def send(
            self, request_proto: Message | None = None, /, **request_fields
        ) -> None:
            ClientStreamingCall.send(self, request_proto, **request_fields)

        _apply_protobuf_signature(self.method, send)

        return type(
            f'{self.method.name}_{base.__name__}', (base,), dict(send=send)
        )


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
        inspect.Parameter('pw_rpc_timeout_s', inspect.Parameter.KEYWORD_ONLY)
    )

    function.__signature__ = sig.replace(  # type: ignore[attr-defined]
        parameters=params
    )


class _UnaryMethodClient(_MethodClient):
    def invoke(
        self,
        request: Message | None = None,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
        *,
        request_args: dict[str, Any] | None = None,
        timeout_s: OptionalTimeout = UseDefault.VALUE,
    ) -> UnaryCall:
        """Invokes the unary RPC and returns a call object."""
        return self._start_call(
            UnaryCall,
            self.method.get_request(request, request_args),
            timeout_s,
            on_next,
            on_completed,
            on_error,
            max_responses=1,
        )

    def open(
        self,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
    ) -> UnaryCall:
        """Invokes the unary RPC and returns a call object."""
        return self._open_call(
            UnaryCall, on_next, on_completed, on_error, max_responses=1
        )


class _ServerStreamingMethodClient(_MethodClient):
    def invoke(
        self,
        request: Message | None = None,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
        max_responses: int = DEFAULT_MAX_STREAM_RESPONSES,
        *,
        request_args: dict[str, Any] | None = None,
        timeout_s: OptionalTimeout = UseDefault.VALUE,
    ) -> ServerStreamingCall:
        """Invokes the server streaming RPC and returns a call object."""
        return self._start_call(
            ServerStreamingCall,
            self.method.get_request(request, request_args),
            timeout_s,
            on_next,
            on_completed,
            on_error,
            max_responses=max_responses,
        )

    def open(
        self,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
        max_responses: int = DEFAULT_MAX_STREAM_RESPONSES,
    ) -> ServerStreamingCall:
        """Returns a call object for the RPC, even if the RPC cannot be invoked.

        Can be used to listen for responses from an RPC server that may yet be
        available.
        """
        return self._open_call(
            ServerStreamingCall, on_next, on_completed, on_error, max_responses
        )


class _ClientStreamingMethodClient(_MethodClient):
    def invoke(
        self,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
        *,
        timeout_s: OptionalTimeout = UseDefault.VALUE,
    ) -> ClientStreamingCall:
        """Invokes the client streaming RPC and returns a call object"""
        return self._start_call(
            self._client_streaming_call_type(ClientStreamingCall),
            None,
            timeout_s,
            on_next,
            on_completed,
            on_error,
            max_responses=1,
        )

    def open(
        self,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
    ) -> ClientStreamingCall:
        """Returns a call object for the RPC, even if the RPC cannot be invoked.

        Can be used to listen for responses from an RPC server that may yet be
        available.
        """
        return self._open_call(
            self._client_streaming_call_type(ClientStreamingCall),
            on_next,
            on_completed,
            on_error,
            max_responses=1,
        )

    def __call__(
        self,
        requests: Iterable[Message] = (),
        *,
        timeout_s: OptionalTimeout = UseDefault.VALUE,
    ) -> UnaryResponse:
        # Instance of 'Uninferable_ClientStreamingCall' has no 'finish_and_wait'
        # pylint: disable-next=no-member
        return self.invoke().finish_and_wait(requests, timeout_s=timeout_s)


class _BidirectionalStreamingMethodClient(_MethodClient):
    def invoke(
        self,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
        max_responses: int = DEFAULT_MAX_STREAM_RESPONSES,
        *,
        timeout_s: OptionalTimeout = UseDefault.VALUE,
    ) -> BidirectionalStreamingCall:
        """Invokes the bidirectional streaming RPC and returns a call object."""
        return self._start_call(
            self._client_streaming_call_type(BidirectionalStreamingCall),
            None,
            timeout_s,
            on_next,
            on_completed,
            on_error,
            max_responses=max_responses,
        )

    def open(
        self,
        on_next: OnNextCallback | None = None,
        on_completed: OnCompletedCallback | None = None,
        on_error: OnErrorCallback | None = None,
        max_responses: int = DEFAULT_MAX_STREAM_RESPONSES,
    ) -> BidirectionalStreamingCall:
        """Returns a call object for the RPC, even if the RPC cannot be invoked.

        Can be used to listen for responses from an RPC server that may yet be
        available.
        """
        return self._open_call(
            self._client_streaming_call_type(BidirectionalStreamingCall),
            on_next,
            on_completed,
            on_error,
            max_responses=max_responses,
        )

    def __call__(
        self,
        requests: Iterable[Message] = (),
        *,
        timeout_s: OptionalTimeout = UseDefault.VALUE,
    ) -> StreamResponse:
        # Instance of 'Uninferable_BidirectionalStreamingCall' has no
        # 'finish_and_wait' member
        # pylint: disable-next=no-member
        return self.invoke().finish_and_wait(requests, timeout_s=timeout_s)


def _method_client_docstring(method: Method) -> str:
    return f'''\
Class that invokes the {method.full_name} {method.type.sentence_name()} RPC.

Calling this directly invokes the RPC synchronously. The RPC can be invoked
asynchronously using the invoke method.
'''


class Impl(client.ClientImpl):
    """Callback-based ClientImpl, for use with pw_rpc.Client.

    Args:
        on_call_hook: A callable object to handle RPC method calls.
            If hook is set, it will be called before RPC execution.
    """

    def __init__(
        self,
        default_unary_timeout_s: float | None = None,
        default_stream_timeout_s: float | None = None,
        on_call_hook: Callable[[CallInfo], Any] | None = None,
    ) -> None:
        super().__init__()
        self._default_unary_timeout_s = default_unary_timeout_s
        self._default_stream_timeout_s = default_stream_timeout_s
        self.on_call_hook = on_call_hook

    @property
    def default_unary_timeout_s(self) -> float | None:
        return self._default_unary_timeout_s

    @property
    def default_stream_timeout_s(self) -> float | None:
        return self._default_stream_timeout_s

    def method_client(self, channel: Channel, method: Method) -> _MethodClient:
        """Returns an object that invokes a method using the given chanel."""

        if method.type is Method.Type.UNARY:
            return self._create_unary_method_client(
                channel, method, self.default_unary_timeout_s
            )

        if method.type is Method.Type.SERVER_STREAMING:
            return self._create_server_streaming_method_client(
                channel, method, self.default_stream_timeout_s
            )

        if method.type is Method.Type.CLIENT_STREAMING:
            return self._create_method_client(
                _ClientStreamingMethodClient,
                channel,
                method,
                self.default_unary_timeout_s,
            )

        if method.type is Method.Type.BIDIRECTIONAL_STREAMING:
            return self._create_method_client(
                _BidirectionalStreamingMethodClient,
                channel,
                method,
                self.default_stream_timeout_s,
            )

        raise AssertionError(f'Unknown method type {method.type}')

    def _create_method_client(
        self,
        base: type,
        channel: Channel,
        method: Method,
        default_timeout_s: float | None,
        **fields,
    ):
        """Creates a _MethodClient derived class customized for the method."""
        method_client_type = type(
            f'{method.name}{base.__name__}',
            (base,),
            dict(__doc__=_method_client_docstring(method), **fields),
        )
        return method_client_type(
            self, self.rpcs, channel, method, default_timeout_s
        )

    def _create_unary_method_client(
        self,
        channel: Channel,
        method: Method,
        default_timeout_s: float | None,
    ) -> _UnaryMethodClient:
        """Creates a _UnaryMethodClient with a customized __call__ method."""

        def call(
            self: _UnaryMethodClient,
            request_proto: Message | None = None,
            /,
            *,
            pw_rpc_timeout_s: OptionalTimeout = UseDefault.VALUE,
            **request_fields,
        ) -> UnaryResponse:
            return self.invoke(
                self.method.get_request(request_proto, request_fields)
            ).wait(pw_rpc_timeout_s)

        _update_call_method(method, call)
        return self._create_method_client(
            _UnaryMethodClient,
            channel,
            method,
            default_timeout_s,
            __call__=call,
        )

    def _create_server_streaming_method_client(
        self,
        channel: Channel,
        method: Method,
        default_timeout_s: float | None,
    ) -> _ServerStreamingMethodClient:
        """Creates _ServerStreamingMethodClient with custom __call__ method."""

        def call(
            self: _ServerStreamingMethodClient,
            request_proto: Message | None = None,
            /,
            *,
            pw_rpc_timeout_s: OptionalTimeout = UseDefault.VALUE,
            **request_fields,
        ) -> StreamResponse:
            return self.invoke(
                self.method.get_request(request_proto, request_fields)
            ).wait(pw_rpc_timeout_s)

        _update_call_method(method, call)
        return self._create_method_client(
            _ServerStreamingMethodClient,
            channel,
            method,
            default_timeout_s,
            __call__=call,
        )

    def handle_response(
        self,
        rpc: PendingRpc,
        context: Call,
        payload,
    ) -> None:
        """Invokes the callback associated with this RPC."""
        context._handle_response(payload)  # pylint: disable=protected-access

    def handle_completion(
        self,
        rpc: PendingRpc,
        context: Call,
        status: Status,
    ):
        context._handle_completion(status)  # pylint: disable=protected-access

    def handle_error(
        self,
        rpc: PendingRpc,
        context: Call,
        status: Status,
    ) -> None:
        context._handle_error(status)  # pylint: disable=protected-access
