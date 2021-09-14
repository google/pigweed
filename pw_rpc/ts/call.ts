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
import {Status} from 'pigweed/pw_status/ts/status';

import {PendingCalls, Rpc} from './rpc_classes';

export type Callback = (a: any) => any;

class RpcError extends Error {
  constructor(rpc: Rpc, status: Status) {
    let message = '';
    if (status === Status.NOT_FOUND) {
      message = ': the RPC server does not support this RPC';
    } else if (status === Status.DATA_LOSS) {
      message = ': an error occurred while decoding the RPC payload';
    }

    super(`${rpc.method} failed with error ${Status[status]}${message}`);
  }
}

/** Represent an in-progress or completed RPC call. */
export class Call {
  private rpcs: PendingCalls;
  private rpc: Rpc;

  private onNext: Callback;
  private onCompleted: Callback;
  private onError: Callback;

  // TODO(jaredweinstein): support async timeout.
  // private timeout: number;
  private status?: Status;
  error?: Status;
  callbackException?: Error;

  constructor(
      rpcs: PendingCalls,
      rpc: Rpc,
      onNext: Callback,
      onCompleted: Callback,
      onError: Callback) {
    this.rpcs = rpcs;
    this.rpc = rpc;

    this.onNext = onNext;
    this.onCompleted = onCompleted;
    this.onError = onError;
  }

  /* Calls the RPC. This must be called immediately after construction. */
  invoke(request?: Message): void {
    const previous = this.rpcs.sendRequest(this.rpc, this, request);

    if (previous !== undefined && !previous.completed()) {
      previous.handleError(Status.CANCELLED)
    }
  }

  private invokeCallback(f: any) {
    try {
      f();
    } catch (err) {
      console.error(
          `An exception was raised while invoking a callback: ${err}`);
      this.callbackException = err
    }
  }

  completed(): boolean {
    return (this.status !== undefined || this.error !== undefined);
  }

  handleResponse(response: Message): void {
    const callback = () => this.onNext(response);
    this.invokeCallback(callback)
  }

  handleCompletion(status: Status) {
    const callback = () => this.onCompleted(status);
    this.invokeCallback(callback)
  }

  handleError(error: Status): void {
    this.error = error;
    this.invokeCallback(() => this.onError(error));
  }

  cancel(): boolean {
    if (this.completed()) {
      return false;
    }

    this.error = Status.CANCELLED
    return this.rpcs.sendCancel(this.rpc);
  }


  private checkErrors(): void {
    if (this.callbackException !== undefined) {
      throw this.callbackException;
    }
    if (this.error !== undefined) {
      throw new RpcError(this.rpc, this.error);
    }
  }
}


/** Tracks the state of a unary RPC call. */
export class UnaryCall extends Call {
  // TODO(jaredweinstein): Complete unary invocation logic.
}

/** Tracks the state of a client streaming RPC call. */
export class ClientStreamingCall extends Call {
  // TODO(jaredweinstein): Complete client streaming invocation logic.
}

/** Tracks the state of a server streaming RPC call. */
export class ServerStreamingCall extends Call {
  // TODO(jaredweinstein): Complete server streaming invocation logic.
}

/** Tracks the state of a bidirectional streaming RPC call. */
export class BidirectionalStreamingCall extends Call {
  // TODO(jaredweinstein): Complete bidirectional streaming invocation logic.
}
