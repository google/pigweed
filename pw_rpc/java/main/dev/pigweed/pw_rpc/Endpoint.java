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
import dev.pigweed.pw_rpc.internal.Packet.RpcPacket;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiFunction;
import java.util.stream.Collectors;
import javax.annotation.Nullable;

/**
 * Tracks the state of service method invocations.
 *
 * The RPC manager handles all RPC-related events and actions. It synchronizes interactions between
 * the endpoint and any threads interacting with RPC call objects.
 */
class Endpoint {
  private static final Logger logger = Logger.forClass(Endpoint.class);

  private final Map<Integer, Channel> channels;
  private final Map<PendingRpc, AbstractCall<?, ?>> pending = new HashMap<>();

  public Endpoint(List<Channel> channels) {
    this.channels = channels.stream().collect(Collectors.toMap(Channel::id, c -> c));
  }

  /**
   * Creates an RPC call object and invokes the RPC
   *
   * @param channelId the channel to use
   * @param method the service method to invoke
   * @param createCall function that creates the call object
   * @param request the request proto; null if this is a client streaming RPC
   * @throws InvalidRpcChannelException if channelId is invalid
   */
  <RequestT extends MessageLite, CallT extends AbstractCall<?, ?>> CallT invokeRpc(int channelId,
      Method method,
      BiFunction<Endpoint, PendingRpc, CallT> createCall,
      @Nullable RequestT request) throws ChannelOutputException {
    CallT call = createCall(channelId, method, createCall);

    // Attempt to start the call.
    logger.atFiner().log("Starting %s", call);

    try {
      // If sending the packet fails, the RPC is never considered pending.
      call.rpc().channel().send(Packets.request(call.rpc(), request));
    } catch (ChannelOutputException e) {
      call.handleExceptionOnInitialPacket(e);
    }
    registerCall(call);
    return call;
  }

  /**
   * Starts listening to responses for an RPC locally, but does not send any packets.
   *
   * <p>The RPC remains open until it is closed by the server (either from a response or error
   * packet) or cancelled.
   */
  <CallT extends AbstractCall<?, ?>> CallT openRpc(
      int channelId, Method method, BiFunction<Endpoint, PendingRpc, CallT> createCall) {
    CallT call = createCall(channelId, method, createCall);
    logger.atFiner().log("Opening %s", call);
    registerCall(call);
    return call;
  }

  private <CallT extends AbstractCall<?, ?>> CallT createCall(
      int channelId, Method method, BiFunction<Endpoint, PendingRpc, CallT> createCall) {
    Channel channel = channels.get(channelId);
    if (channel == null) {
      throw InvalidRpcChannelException.unknown(channelId);
    }

    return createCall.apply(this, PendingRpc.create(channel, method));
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

  public synchronized boolean processClientPacket(@Nullable Method method, RpcPacket packet) {
    Channel channel = channels.get(packet.getChannelId());
    if (channel == null) {
      logger.atWarning().log("Received packet for unrecognized channel %d", packet.getChannelId());
      return false;
    }

    if (method == null) {
      logger.atFine().log("Ignoring packet for unknown service method");
      sendError(channel, packet, Status.NOT_FOUND);
      return true; // true since the packet was handled, even though it was invalid.
    }

    PendingRpc rpc = PendingRpc.create(channel, method);
    if (!updateCall(packet, rpc)) {
      logger.atFine().log("Ignoring packet for %s, which isn't pending", rpc);
      sendError(channel, packet, Status.FAILED_PRECONDITION);
      return true;
    }

    return true;
  }

  /** Returns true if the packet was forwarded to an active RPC call; false if no call was found. */
  private boolean updateCall(RpcPacket packet, PendingRpc rpc) {
    switch (packet.getType()) {
      case SERVER_ERROR: {
        Status status = decodeStatus(packet);
        return handleError(rpc, status);
      }
      case RESPONSE: {
        Status status = decodeStatus(packet);
        // Client streaming and unary RPCs include a payload with their response packet.
        if (rpc.method().isServerStreaming()) {
          return handleStreamCompleted(rpc, status);
        }
        return handleUnaryCompleted(rpc, packet.getPayload(), status);
      }
      case SERVER_STREAM:
        return handleNext(rpc, packet.getPayload());
      default:
        logger.atWarning().log(
            "%s received unexpected PacketType %d", rpc, packet.getType().getNumber());
    }

    return true;
  }

  private static void sendError(Channel channel, RpcPacket packet, Status status) {
    try {
      channel.send(Packets.error(packet, status));
    } catch (ChannelOutputException e) {
      logger.atWarning().withCause(e).log("Failed to send error packet");
    }
  }

  private static Status decodeStatus(RpcPacket packet) {
    Status status = Status.fromCode(packet.getStatus());
    if (status == null) {
      logger.atWarning().log(
          "Illegal status code %d in packet; using Status.UNKNOWN ", packet.getStatus());
      return Status.UNKNOWN;
    }
    return status;
  }
}
