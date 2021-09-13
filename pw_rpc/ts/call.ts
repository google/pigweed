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

import {Status} from 'pigweed/pw_status/ts/status';

import {PendingCalls, Rpc} from './rpc_classes';

export type Callback = (a: any) => any;

/** Represent an in-progress or completed RPC call. */
export class Call {
  private rpcs: PendingCalls;
  private rpc: Rpc;

  private onNext: Callback;
  private onCompleted: Callback;
  private onError: Callback;

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
