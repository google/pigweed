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

import com.google.common.flogger.FluentLogger;
import com.google.protobuf.MessageLite;
import java.util.HashMap;
import java.util.Map;
import javax.annotation.Nullable;

/** Tracks the state of service method invocations. */
public class RpcManager {
  private static final FluentLogger logger = FluentLogger.forEnclosingClass();
  private final Map<PendingRpc, StreamObserverCall<?, ?>> pending = new HashMap<>();

  /**
   * Invokes an RPC.
   *
   * @param rpc channel / service / method tuple that unique identifies this RPC
   * @param call object for this RPC
   * @param payload the request
   */
  @Nullable
  public synchronized StreamObserverCall<?, ?> start(
      PendingRpc rpc, StreamObserverCall<?, ?> call, @Nullable MessageLite payload)
      throws ChannelOutputException {
    logger.atFine().log("Start %s", rpc);
    rpc.channel().send(Packets.request(rpc, payload));
    return pending.put(rpc, call);
  }

  /**
   * Invokes an RPC, but ignores errors and keeps the RPC active if the invocation fails.
   *
   * <p>The RPC remains open until it is closed by the server (either with a response or error
   * packet) or cancelled.
   */
  @Nullable
  public synchronized StreamObserverCall<?, ?> open(
      PendingRpc rpc, StreamObserverCall<?, ?> call, @Nullable MessageLite payload) {
    logger.atFine().log("Open %s", rpc);
    try {
      rpc.channel().send(Packets.request(rpc, payload));
    } catch (ChannelOutputException e) {
      logger.atFine().withCause(e).log(
          "Ignoring error opening %s; listening for unrequested responses", rpc);
    }
    return pending.put(rpc, call);
  }

  /** Cancels an ongoing RPC */
  @Nullable
  public synchronized StreamObserverCall<?, ?> cancel(PendingRpc rpc)
      throws ChannelOutputException {
    StreamObserverCall<?, ?> call = pending.remove(rpc);
    if (call != null) {
      logger.atFine().log("Cancel %s", rpc);
      rpc.channel().send(Packets.cancel(rpc));
    }
    return call;
  }

  @Nullable
  public synchronized StreamObserverCall<?, ?> clientStream(PendingRpc rpc, MessageLite payload)
      throws ChannelOutputException {
    StreamObserverCall<?, ?> call = pending.get(rpc);
    if (call != null) {
      rpc.channel().send(Packets.clientStream(rpc, payload));
    }
    return call;
  }

  @Nullable
  public synchronized StreamObserverCall<?, ?> clientStreamEnd(PendingRpc rpc)
      throws ChannelOutputException {
    StreamObserverCall<?, ?> call = pending.get(rpc);
    if (call != null) {
      rpc.channel().send(Packets.clientStreamEnd(rpc));
    }
    return call;
  }

  @Nullable
  public synchronized StreamObserverCall<?, ?> clear(PendingRpc rpc) {
    StreamObserverCall<?, ?> call = pending.remove(rpc);
    if (call != null) {
      logger.atFine().log("Clear %s", rpc);
    }
    return call;
  }

  @Nullable
  public synchronized StreamObserverCall<?, ?> getPending(PendingRpc rpc) {
    return pending.get(rpc);
  }

  @Override
  public synchronized String toString() {
    return "RpcManager{"
        + "pending=" + pending + '}';
  }
}
