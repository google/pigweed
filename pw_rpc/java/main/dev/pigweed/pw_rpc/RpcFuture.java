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

import com.google.common.util.concurrent.AbstractFuture;
import com.google.protobuf.MessageLite;
import java.util.function.Consumer;
import javax.annotation.Nullable;

/** Used to wait for an RPC to finish with a ListenableFuture. */
abstract class RpcFuture<ResponseT extends MessageLite, ResultT>
    extends AbstractFuture<ResultT> implements StreamObserver<ResponseT> {
  private final PendingRpc rpc;

  RpcFuture(PendingRpc rpc) {
    this.rpc = rpc;
  }

  @Override
  public void onError(Status status) {
    setException(new RpcError(rpc, status));
  }

  /** Future for unary and client streaming RPCs. */
  static class Unary<ResponseT extends MessageLite>
      extends RpcFuture<ResponseT, UnaryResult<ResponseT>> {
    @Nullable ResponseT response = null;

    Unary(PendingRpc rpc) {
      super(rpc);
    }

    @Override
    public void onNext(ResponseT value) {
      if (response == null) {
        response = value;
      } else {
        setException(new IllegalStateException("Unary RPC received multiple responses."));
      }
    }

    @Override
    public void onCompleted(Status status) {
      if (response == null) {
        setException(new IllegalStateException("Unary RPC completed without a response payload"));
      } else {
        set(UnaryResult.create(response, status));
      }
    }
  }

  /**
   * Future for server and bidirectional streaming RPCs.
   *
   * <p>Takes a callback to execute for each response.
   */
  static class Stream<ResponseT extends MessageLite> extends RpcFuture<ResponseT, Status> {
    private final Consumer<ResponseT> onNext;

    Stream(PendingRpc rpc, Consumer<ResponseT> onNext) {
      super(rpc);
      this.onNext = onNext;
    }

    @Override
    public void onNext(ResponseT value) {
      onNext.accept(value);
    }

    @Override
    public void onCompleted(Status status) {
      set(status);
    }
  }
}
