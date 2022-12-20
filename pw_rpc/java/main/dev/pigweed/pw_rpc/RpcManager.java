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

import com.google.protobuf.ByteString;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_log.Logger;
import java.util.HashMap;
import java.util.Map;
import javax.annotation.Nullable;

/**
 * Tracks the state of service method invocations.
 *
 * The RPC manager handles all RPC-related events and actions. It synchronizes interactions between
 * the endpoint and any threads interacting with RPC call objects.
 */
class RpcManager {
  private static final Logger logger = Logger.forClass(RpcManager.class);

  private final Map<PendingRpc, AbstractCall<?, ?>> pending = new HashMap<>();

  /**
   * Invokes an RPC.
   *
   * @param call object for this RPC
   * @param payload the request
   */
  public synchronized void start(AbstractCall<?, ?> call, @Nullable MessageLite payload)
      throws ChannelOutputException {
    logger.atFiner().log("Starting %s", call);
    // If sending the packet fails, the RPC is never considered pending.
    call.rpc().channel().send(Packets.request(call.rpc(), payload));
    registerCall(call);
  }

  /**
   * Listens to responses to an RPC without sending a request.
   *
   * <p>The RPC remains open until it is closed by the server (either with a response or error
   * packet) or cancelled.
   */
  public synchronized void open(AbstractCall<?, ?> call) {
    logger.atFiner().log("Opening %s", call);
    registerCall(call);
  }

  private void registerCall(AbstractCall<?, ?> call) {
    // TODO(hepler): Use call_id to support simultaneous calls for the same RPC on one channel.
    //
    // Originally, only one call per service/method/channel was supported. With this restriction,
    // the original call should have been aborted here, but was not. The client will be updated to
    // support multiple simultaneous calls instead of aborting the call.
    pending.put(call.rpc(), call);
  }

  /** Cancels an ongoing RPC */
  public synchronized boolean cancel(AbstractCall<?, ?> call) throws ChannelOutputException {
    if (pending.remove(call.rpc()) == null) {
      return false;
    }
    logger.atFiner().log("Cancelling %s", call);
    call.handleError(Status.CANCELLED);
    call.sendPacket(Packets.cancel(call.rpc()));
    return true;
  }

  /** Cancels an ongoing RPC without sending a cancellation packet. */
  public synchronized boolean abandon(AbstractCall<?, ?> call) {
    if (pending.remove(call.rpc()) == null) {
      return false;
    }
    logger.atFiner().log("Abandoning %s", call);
    call.handleError(Status.CANCELLED);
    return true;
  }

  public synchronized boolean clientStream(AbstractCall<?, ?> call, MessageLite payload)
      throws ChannelOutputException {
    return sendPacket(call, Packets.clientStream(call.rpc(), payload));
  }

  public synchronized boolean clientStreamEnd(AbstractCall<?, ?> call)
      throws ChannelOutputException {
    return sendPacket(call, Packets.clientStreamEnd(call.rpc()));
  }

  private boolean sendPacket(AbstractCall<?, ?> call, byte[] packet) throws ChannelOutputException {
    if (!pending.containsKey(call.rpc())) {
      return false;
    }
    // TODO(hepler): Consider aborting the call if sending the packet fails.
    call.sendPacket(packet);
    return true;
  }

  public synchronized boolean handleNext(PendingRpc rpc, ByteString payload) {
    AbstractCall<?, ?> call = pending.get(rpc);
    if (call == null) {
      return false;
    }
    call.handleNext(payload);
    logger.atFiner().log("%s received server stream with %d B payload", call, payload.size());
    return true;
  }

  public synchronized boolean handleUnaryCompleted(
      PendingRpc rpc, ByteString payload, Status status) {
    AbstractCall<?, ?> call = pending.remove(rpc);
    if (call == null) {
      return false;
    }
    call.handleUnaryCompleted(payload, status);
    logger.atFiner().log(
        "%s completed with status %s and %d B payload", call, status, payload.size());
    return true;
  }

  public synchronized boolean handleStreamCompleted(PendingRpc rpc, Status status) {
    AbstractCall<?, ?> call = pending.remove(rpc);
    if (call == null) {
      return false;
    }
    call.handleStreamCompleted(status);
    logger.atFiner().log("%s completed with status %s", call, status);
    return true;
  }

  public synchronized boolean handleError(PendingRpc rpc, Status status) {
    AbstractCall<?, ?> call = pending.remove(rpc);
    if (call == null) {
      return false;
    }
    call.handleError(status);
    logger.atFiner().log("%s failed with error %s", call, status);
    return true;
  }
}
