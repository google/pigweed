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
import WaitQueue = require('wait-queue')

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
  // Responses ordered by arrival time. Undefined signifies stream completion.
  private responseQueue = new WaitQueue<Message|undefined>();

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
    this.responseQueue.push(response);
    const callback = () => this.onNext(response);
    this.invokeCallback(callback)
  }

  handleCompletion(status: Status) {
    this.status = status;
    this.responseQueue.push(undefined);
    const callback = () => this.onCompleted(status);
    this.invokeCallback(callback)
  }

  handleError(error: Status): void {
    this.error = error;
    this.responseQueue.push(undefined);
    this.invokeCallback(() => this.onError(error));
  }

  /**
   * Yields responses up the specified count as they are added.
   *
   * Throws an error as soon as it is received even if there are still responses
   * in the queue.
   *
   * Usage
   * ```
   * for await (const response of call.getResponses(5)) {
   *  console.log(response);
   * }
   * ```
   *
   * @param {number} count The number of responses to read before returning.
   *    If no value is specified, getResponses will block until the stream
   *    either ends or hits an error.
   */
  async * getResponses(count?: number): AsyncGenerator<Message> {
    this.checkErrors();

    if (this.completed() && this.responseQueue.length == 0) {
      return;
    }

    let remaining = count ?? Number.POSITIVE_INFINITY;
    while (remaining > 0) {
      const response = await this.responseQueue.shift();
      if (response === undefined) {
        return;
      }
      this.checkErrors();
      yield response!;
      remaining -= 1;
    }
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
