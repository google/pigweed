// Copyright 2022 The Pigweed Authors
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
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Call.ClientStreaming;
import java.util.Locale;
import javax.annotation.Nullable;

/**
 * Partial implementation of the Call interface.
 *
 * Call objects never manipulate their own state through public functions. They only manipulate
 * state through functions called by the RpcManager class.
 */
abstract class AbstractCall<RequestT extends MessageLite, ResponseT extends MessageLite>
    implements Call, ClientStreaming<RequestT> {
  private static final Logger logger = Logger.forClass(StreamObserverCall.class);

  private final RpcManager rpcs;
  private final PendingRpc rpc;

  @Nullable private Status status = null;
  @Nullable private Status error = null;

  AbstractCall(RpcManager rpcs, PendingRpc rpc) {
    this.rpcs = rpcs;
    this.rpc = rpc;
  }

  @Nullable
  @Override
  public final Status status() {
    return status;
  }

  @Nullable
  @Override
  public final Status error() {
    return error;
  }

  @Override
  public final boolean cancel() throws ChannelOutputException {
    return rpcs.cancel(this);
  }

  @Override
  public final boolean abandon() {
    return rpcs.abandon(this);
  }

  @Override
  public final boolean write(RequestT request) throws ChannelOutputException {
    return rpcs.clientStream(this, request);
  }

  @Override
  public final boolean finish() throws ChannelOutputException {
    return rpcs.clientStreamEnd(this);
  }

  final void sendPacket(byte[] packet) throws ChannelOutputException {
    rpc.channel().send(packet);
  }

  final PendingRpc rpc() {
    return rpc;
  }

  final RpcManager rpcManager() {
    return rpcs;
  }

  // The following functions change the call's state and may ONLY be called by the RpcManager!

  final void handleNext(ByteString payload) {
    ResponseT response = parseResponse(payload);
    if (response != null) {
      doHandleNext(response);
    }
  }

  abstract void doHandleNext(ResponseT response);

  final void handleStreamCompleted(Status status) {
    this.status = status;
    doHandleCompleted();
  }

  final void handleUnaryCompleted(ByteString payload, Status status) {
    this.status = status;
    handleNext(payload);
    doHandleCompleted();
  }

  abstract void doHandleCompleted();

  final void handleError(Status error) {
    this.error = error;
    doHandleError();
  }

  abstract void doHandleError();

  @SuppressWarnings("unchecked")
  @Nullable
  private ResponseT parseResponse(ByteString payload) {
    try {
      return (ResponseT) rpc.method().decodeResponsePayload(payload);
    } catch (InvalidProtocolBufferException e) {
      logger.atWarning().withCause(e).log(
          "Failed to decode response for method %s; skipping packet", rpc.method().name());
      return null;
    }
  }

  @Override
  public String toString() {
    return String.format(Locale.ENGLISH,
        "RpcCall[%s|channel=%d|%s]",
        rpc.method(),
        rpc.channel().id(),
        active() ? "active" : "inactive");
  }
}
