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
  private final Map<PendingRpc, Object> pending = new HashMap<>();

  /**
   * Invokes an RPC.
   *
   * @param rpc channel / service / method tuple that unique identifies this RPC
   * @param context arbitrary implementation-defined non-null value to associate with the RPC
   * @param payload the request
   */
  @Nullable
  public synchronized Object start(PendingRpc rpc, Object context, @Nullable MessageLite payload)
      throws ChannelOutputException {
    logger.atFine().log("Start %s", rpc);
    rpc.channel().send(Packets.request(rpc, payload));
    return pending.put(rpc, context);
  }

  /**
   * Invokes an RPC, but ignores errors and keeps the RPC active if the invocation fails.
   *
   * <p>The RPC remains open until it is closed by the server (either with a response or error
   * packet) or cancelled.
   */
  @Nullable
  public synchronized Object open(PendingRpc rpc, Object context, @Nullable MessageLite payload) {
    logger.atFine().log("Open %s", rpc);
    try {
      rpc.channel().send(Packets.request(rpc, payload));
    } catch (ChannelOutputException e) {
      logger.atFine().withCause(e).log(
          "Ignoring error opening %s; listening for unrequested responses", rpc);
    }
    return pending.put(rpc, context);
  }

  /** Cancels an ongoing RPC */
  @Nullable
  public synchronized Object cancel(PendingRpc rpc) throws ChannelOutputException {
    Object context = pending.remove(rpc);
    if (context != null) {
      logger.atFine().log("Cancel %s", rpc);
      rpc.channel().send(Packets.cancel(rpc));
    }
    return context;
  }

  @Nullable
  public synchronized Object clientStream(PendingRpc rpc, MessageLite payload)
      throws ChannelOutputException {
    Object context = pending.get(rpc);
    if (context != null) {
      rpc.channel().send(Packets.clientStream(rpc, payload));
    }
    return context;
  }

  @Nullable
  public synchronized Object clientStreamEnd(PendingRpc rpc) throws ChannelOutputException {
    Object context = pending.get(rpc);
    if (context != null) {
      rpc.channel().send(Packets.clientStreamEnd(rpc));
    }
    return context;
  }

  @Nullable
  public synchronized Object clear(PendingRpc rpc) {
    Object context = pending.remove(rpc);
    if (context != null) {
      logger.atFine().log("Clear %s", rpc);
    }
    return context;
  }

  @Nullable
  public synchronized Object getPending(PendingRpc rpc) {
    return pending.get(rpc);
  }

  @Override
  public synchronized String toString() {
    return "RpcManager{"
        + "pending=" + pending + '}';
  }
}
