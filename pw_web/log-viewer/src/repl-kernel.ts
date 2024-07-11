// Copyright 2024 The Pigweed Authors
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

export interface RPCPayload {
  id: number;
  type: string;
  data: any;
  streaming?: boolean;
  close?: boolean;
}
export type RPCStreamHandler = (data: any) => void;
export type RPCCallback = (data: any, streaming?: boolean) => void;
export type RPCUnsubscribeStream = () => void;

export abstract class RPCClient {
  private callbacks: { [key: number]: RPCCallback } = {};
  private latest_call_id = 1;
  public abstract send(payload: RPCPayload): void;
  public call(requestType: string, data?: any): Promise<any> {
    const payload: RPCPayload = {
      id: this.latest_call_id++,
      type: requestType,
      data,
    };
    return new Promise((resolve, _reject) => {
      this.callbacks[payload.id] = resolve;
      this.send(payload);
    });
  }

  public openStream(
    requestType: string,
    data: any,
    streamHandler: RPCStreamHandler,
  ): Promise<RPCUnsubscribeStream> {
    const payload = {
      id: this.latest_call_id++,
      type: requestType,
      data,
    };

    const unsub = () => {
      return new Promise((resolve, _reject) => {
        this.callbacks[payload.id] = resolve;
        this.send({ ...payload, close: true });
      });
    };
    return new Promise((resolve, _reject) => {
      this.callbacks[payload.id] = streamHandler;
      this.send(payload);
      resolve(unsub);
    });
  }

  // Check if this is a response to RPC call
  handleResponse(response: RPCPayload) {
    try {
      if (response.id && this.callbacks[response.id]) {
        this.callbacks[response.id](response.data, response.streaming);
        if (!response.streaming) delete this.callbacks[response.id];
        return true;
      } else if (response.id) {
        console.error('callback not found for', response);
      }
    } catch (e) {
      console.log(e);
    }
    return false;
  }
}

export class WebSocketRPCClient extends RPCClient {
  private pendingCalls: RPCPayload[] = [];
  private isConnected = false;
  constructor(private ws: WebSocket) {
    super();

    this.ws.onopen = () => {
      this.isConnected = true;
      if (this.pendingCalls.length > 0) {
        this.pendingCalls.forEach(this.send.bind(this));
      }
    };

    this.ws.onclose = () => {
      this.isConnected = false;
    };

    this.ws.onmessage = (event) => {
      this.handleResponse(JSON.parse(event.data));
    };
  }
  send(payload: RPCPayload) {
    if (!this.isConnected) {
      this.pendingCalls.push(payload);
      return;
    }
    this.ws.send(JSON.stringify(payload));
  }
}

export interface EvalOutput {
  stdout?: string;
  stderr?: string;
  result?: string;
}

export interface AutocompleteSuggestion {
  text: string;
  type: string;
}

export type LogHandler = (text: string) => void;
export type UnsubscribeLogStream = () => void;

export abstract class ReplKernel {
  abstract eval(code: string): Promise<EvalOutput>;
  abstract autocomplete(
    code: string,
    cursorPos: number,
  ): Promise<AutocompleteSuggestion[]>;
}

export class WebSocketRPCReplKernel extends ReplKernel {
  constructor(private rpcClient: WebSocketRPCClient) {
    super();
  }
  eval(code: string) {
    return this.rpcClient.call('eval', { code });
  }
  autocomplete(code: string, cursorPos: number) {
    return this.rpcClient.call('autocomplete', { code, cursor_pos: cursorPos });
  }
}
