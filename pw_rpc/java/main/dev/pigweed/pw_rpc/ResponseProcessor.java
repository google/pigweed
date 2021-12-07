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

package dev.pigweed.pw_rpc;

import com.google.protobuf.MessageLite;

/** Called by the server when it receives a packet for an ongoing RPC. */
public interface ResponseProcessor {
  /**
   * Handles a response from the RPC server.
   *
   * @param rpcs object for sending RPC requests
   * @param rpc reference to the RPC the packet is for
   * @param context the context object provided when the RPC was invoked
   * @param payload decoded response payload
   */
  void onNext(RpcManager rpcs, PendingRpc rpc, Object context, MessageLite payload);

  /**
   * Handles the completion of an RPC.
   *
   * @param rpcs object for sending RPC requests
   * @param rpc reference to the RPC the packet is for
   * @param context the context object provided when the RPC was invoked
   * @param status the status returned by the server
   */
  void onCompleted(RpcManager rpcs, PendingRpc rpc, Object context, Status status);

  /**
   * Handles the abnormal termination of an RPC.
   *
   * @param rpcs object for sending RPC requests
   * @param rpc reference to the RPC the packet is for
   * @param context the context object provided when the RPC was invoked
   * @param status the status returned by the server
   */
  void onError(RpcManager rpcs, PendingRpc rpc, Object context, Status status);
}
