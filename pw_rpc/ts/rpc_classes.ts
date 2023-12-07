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

import { Message } from 'google-protobuf';
import { Status } from 'pigweedjs/pw_status';

import { Call } from './call';
import { Channel, Method, Service } from './descriptors';
import * as packets from './packets';

/** Max number that can fit into a 2-byte varint */
const MAX_CALL_ID = 1 << 14;

/** Data class for a pending RPC call. */
export class Rpc {
  readonly channel: Channel;
  readonly service: Service;
  readonly method: Method;

  constructor(channel: Channel, service: Service, method: Method) {
    this.channel = channel;
    this.service = service;
    this.method = method;
  }

  /** Returns channel service method callId tuple */
  getIdSet(callId: number): [number, number, number, number] {
    return [this.channel.id, this.service.id, this.method.id, callId];
  }

  /**
   * Returns a string sequence to uniquely identify channel, service, method
   * and call ID. This can be used to hash the Rpc.
   *
   * For example: "12346789.23452345.12341234.34"
   */
  getIdString(callId: number): string {
    return `${this.channel.id}.${this.service.id}.${this.method.id}.${callId}`;
  }

  toString(): string {
    return (
      `${this.service.name}.${this.method.name} on channel ` +
      `${this.channel.id}`
    );
  }
}

/** Tracks pending RPCs and encodes outgoing RPC packets. */
export class PendingCalls {
  pending: Map<string, Call> = new Map();
  nextCallId: number = 0;

  /** Starts the provided RPC and returns the encoded packet to send. */
  request(rpc: Rpc, request: Message, call: Call): Uint8Array {
    this.open(rpc, call);
    console.log(`Starting ${rpc}`);
    return packets.encodeRequest(rpc.getIdSet(call.callId), request);
  }

  allocateCallId(): number {
    const callId = this.nextCallId;
    this.nextCallId = (this.nextCallId + 1) % MAX_CALL_ID;
    return callId;
  }

  /** Calls request and sends the resulting packet to the channel. */
  sendRequest(
    rpc: Rpc,
    call: Call,
    ignoreError: boolean,
    request?: Message,
  ): Call | undefined {
    const previous = this.open(rpc, call);
    const packet = packets.encodeRequest(rpc.getIdSet(call.callId), request);
    try {
      rpc.channel.send(packet);
    } catch (error) {
      if (!ignoreError) {
        throw error;
      }
    }
    return previous;
  }

  /**
   * Creates a call for an RPC, but does not invoke it.
   *
   * open() can be used to receive streaming responses to an RPC that was not
   * invoked by this client. For example, a server may stream logs with a
   * server streaming RPC prior to any clients invoking it.
   */
  open(rpc: Rpc, call: Call): Call | undefined {
    console.debug(`Starting ${rpc}`);
    const previous = this.pending.get(rpc.getIdString(call.callId));
    this.pending.set(rpc.getIdString(call.callId), call);
    return previous;
  }

  sendClientStream(rpc: Rpc, message: Message, callId: number) {
    if (this.getPending(rpc, callId) === undefined) {
      throw new Error(`Attempt to send client stream for inactive RPC: ${rpc}`);
    }
    rpc.channel.send(packets.encodeClientStream(rpc.getIdSet(callId), message));
  }

  sendClientStreamEnd(rpc: Rpc, callId: number) {
    if (this.getPending(rpc, callId) === undefined) {
      throw new Error(`Attempt to send client stream for inactive RPC: ${rpc}`);
    }
    rpc.channel.send(packets.encodeClientStreamEnd(rpc.getIdSet(callId)));
  }

  /** Cancels the RPC. Returns the CLIENT_ERROR packet to send. */
  cancel(rpc: Rpc, callId: number): Uint8Array {
    console.debug(`Cancelling ${rpc}`);
    this.pending.delete(rpc.getIdString(callId));
    return packets.encodeCancel(rpc.getIdSet(callId));
  }

  /** Calls cancel and sends the cancel packet, if any, to the channel. */
  sendCancel(rpc: Rpc, callId: number): boolean {
    let packet: Uint8Array | undefined;
    try {
      packet = this.cancel(rpc, callId);
    } catch (err) {
      return false;
    }

    if (packet !== undefined) {
      rpc.channel.send(packet);
    }
    return true;
  }

  /** Gets the pending RPC's call. If status is set, clears the RPC. */
  getPending(rpc: Rpc, callId: number, status?: Status): Call | undefined {
    if (status === undefined) {
      return this.pending.get(rpc.getIdString(callId));
    }

    const call = this.pending.get(rpc.getIdString(callId));
    this.pending.delete(rpc.getIdString(callId));
    return call;
  }
}
