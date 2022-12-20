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
import javax.annotation.Nullable;

/**
 * Represents an ongoing RPC call.
 *
 * <p>This call class implements all features of unary, server streaming, client streaming, and
 * bidirectional streaming RPCs. It provides static methods for creating call objects for each RPC
 * type.
 *
 * @param <RequestT> request type of the RPC; used for client or bidirectional streaming RPCs
 * @param <ResponseT> response type of the RPC; used for all types of RPCs
 */
final class StreamObserverCall<RequestT extends MessageLite, ResponseT extends MessageLite>
    extends AbstractCall<RequestT, ResponseT> {
  private final StreamObserver<ResponseT> observer;

  /** Invokes the specified RPC. */
  static <RequestT extends MessageLite, ResponseT extends MessageLite>
      StreamObserverCall<RequestT, ResponseT> start(Endpoint rpcs,
          PendingRpc rpc,
          StreamObserver<ResponseT> observer,
          @Nullable MessageLite request) throws ChannelOutputException {
    StreamObserverCall<RequestT, ResponseT> call = new StreamObserverCall<>(rpcs, rpc, observer);
    rpcs.start(call, request);
    return call;
  }

  /** Open the specified RPC locally without sending any packets. */
  static <RequestT extends MessageLite, ResponseT extends MessageLite>
      StreamObserverCall<RequestT, ResponseT> open(
          Endpoint rpcs, PendingRpc rpc, StreamObserver<ResponseT> observer) {
    StreamObserverCall<RequestT, ResponseT> call = new StreamObserverCall<>(rpcs, rpc, observer);
    rpcs.open(call);
    return call;
  }

  private StreamObserverCall(Endpoint rpcs, PendingRpc rpc, StreamObserver<ResponseT> observer) {
    super(rpcs, rpc);
    this.observer = observer;
  }

  @Override
  void doHandleNext(ResponseT response) {
    observer.onNext(response);
  }

  @Override
  void doHandleCompleted() {
    observer.onCompleted(status());
  }

  @Override
  void doHandleError() {
    observer.onError(error());
  }
}
