// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

import {Message} from 'google-protobuf';

import {Call, Callback, ServerStreamingCall, UnaryCall} from './call';
import {Channel, Method, MethodType, Service} from './descriptors';
import {PendingCalls, Rpc} from './rpc_classes';

export function methodStubFactory(
    rpcs: PendingCalls, channel: Channel, method: Method): MethodStub {
  switch (method.type) {
    case MethodType.BIDIRECTIONAL_STREAMING:
      return new BidirectionStreamingMethodStub(rpcs, channel, method);
    case MethodType.CLIENT_STREAMING:
      return new ClientStreamingMethodStub(rpcs, channel, method);
    case MethodType.SERVER_STREAMING:
      return new ServerStreamingMethodStub(rpcs, channel, method);
    case MethodType.UNARY:
      return new UnaryMethodStub(rpcs, channel, method);
  }
}

export class MethodStub {
  readonly method: Method;
  readonly rpcs: PendingCalls;
  readonly rpc: Rpc;
  private channel: Channel;

  constructor(rpcs: PendingCalls, channel: Channel, method: Method) {
    this.method = method;
    this.rpcs = rpcs;
    this.channel = channel;
    this.rpc = new Rpc(channel, method.service, method)
  }

  invoke(
      request?: Message,
      onNext: Callback = () => {},
      onCompleted: Callback = () => {},
      onError: Callback = () => {}): UnaryCall {
    throw Error('invoke() not implemented');
  }
}

class UnaryMethodStub extends MethodStub {
  // TODO(jaredweinstein): Add blocking invocation.
  // invokeBlocking(request) {...}

  invoke(
      request?: Message,
      onNext: Callback = () => {},
      onCompleted: Callback = () => {},
      onError: Callback = () => {}): UnaryCall {
    const call =
        new UnaryCall(this.rpcs, this.rpc, onNext, onCompleted, onError);
    call.invoke(request!);
    return call;
  }
}

class ServerStreamingMethodStub extends MethodStub {
  invoke(
      request?: Message,
      onNext: Callback = () => {},
      onCompleted: Callback = () => {},
      onError: Callback = () => {}): ServerStreamingCall {
    const call = new ServerStreamingCall(
        this.rpcs, this.rpc, onNext, onCompleted, onError);
    call.invoke(request);
    return call;
  }
}

class ClientStreamingMethodStub extends MethodStub {
  invoke(
      request: Message,
      onNext: Callback,
      onCompleted: Callback,
      onError: Callback): Call {
    throw Error('ClientStreaming invoke() not implemented');
  }
}

class BidirectionStreamingMethodStub extends MethodStub {
  invoke(
      request: Message,
      onNext: Callback,
      onCompleted: Callback,
      onError: Callback): Call {
    throw Error('BidirectionalStreaming invoke() not implemented');
  }
}
