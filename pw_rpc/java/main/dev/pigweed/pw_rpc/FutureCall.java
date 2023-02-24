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

import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.SettableFuture;
import com.google.protobuf.MessageLite;
import dev.pigweed.pw_log.Logger;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import javax.annotation.Nullable;

/**
 * Call implementation that represents the call as a ListenableFuture.
 *
 * This class suppresses ShouldNotSubclass warnings from ListenableFuture. It implements
 * ListenableFuture only because it cannot extend AbstractFuture since multiple inheritance is not
 * supported. No Future funtionality is duplicated; FutureCall uses SettableFuture internally.
 */
@SuppressWarnings("ShouldNotSubclass")
abstract class FutureCall<RequestT extends MessageLite, ResponseT extends MessageLite, ResultT>
    extends AbstractCall<RequestT, ResponseT> implements ListenableFuture<ResultT> {
  private static final Logger logger = Logger.forClass(FutureCall.class);

  private final SettableFuture<ResultT> future = SettableFuture.create();

  private FutureCall(Endpoint endpoint, PendingRpc rpc) {
    super(endpoint, rpc);
  }

  // Implement the ListenableFuture interface by forwarding the internal SettableFuture.

  @Override
  public final void addListener(Runnable runnable, Executor executor) {
    future.addListener(runnable, executor);
  }

  /** Cancellation means that a cancel() or cancel(boolean) call succeeded. */
  @Override
  public final boolean isCancelled() {
    return error() == Status.CANCELLED;
  }

  @Override
  public final boolean isDone() {
    return future.isDone();
  }

  @Override
  public final ResultT get() throws InterruptedException, ExecutionException {
    return future.get();
  }

  @Override
  public final ResultT get(long timeout, TimeUnit unit)
      throws InterruptedException, ExecutionException, TimeoutException {
    return future.get(timeout, unit);
  }

  @Override
  public final boolean cancel(boolean mayInterruptIfRunning) {
    try {
      return this.cancel();
    } catch (ChannelOutputException e) {
      logger.atWarning().withCause(e).log("Failed to send cancellation packet for %s", rpc());
      return true; // handleError() was already called, so the future was cancelled
    }
  }

  /** Used by derived classes to access the future instance. */
  final SettableFuture<ResultT> future() {
    return future;
  }

  @Override
  void handleExceptionOnInitialPacket(ChannelOutputException e) {
    // Stash the exception in the future and abort the call.
    future.setException(e);

    // Set the status to mark the call completed. doHandleError() will have no effect since the
    // exception was already set.
    handleError(Status.ABORTED);
  }

  @Override
  public void doHandleError() {
    future.setException(new RpcError(rpc(), error()));
  }

  /** Future-based Call class for unary and client streaming RPCs. */
  static class UnaryResponseFuture<RequestT extends MessageLite, ResponseT extends MessageLite>
      extends FutureCall<RequestT, ResponseT, UnaryResult<ResponseT>>
      implements ClientStreamingFuture<RequestT, ResponseT> {
    @Nullable ResponseT response = null;

    UnaryResponseFuture(Endpoint endpoint, PendingRpc rpc) {
      super(endpoint, rpc);
    }

    @Override
    public void doHandleNext(ResponseT value) {
      if (response == null) {
        response = value;
      } else {
        future().setException(new IllegalStateException("Unary RPC received multiple responses."));
      }
    }

    @Override
    public void doHandleCompleted() {
      if (response == null) {
        future().setException(
            new IllegalStateException("Unary RPC completed without a response payload"));
      } else {
        future().set(UnaryResult.create(response, status()));
      }
    }
  }

  /** Future-based Call class for server and bidirectional streaming RPCs. */
  static class StreamResponseFuture<RequestT extends MessageLite, ResponseT extends MessageLite>
      extends FutureCall<RequestT, ResponseT, Status>
      implements BidirectionalStreamingFuture<RequestT> {
    private final Consumer<ResponseT> onNext;

    static <RequestT extends MessageLite, ResponseT extends MessageLite>
        BiFunction<Endpoint, PendingRpc, StreamResponseFuture<RequestT, ResponseT>> getFactory(
            Consumer<ResponseT> onNext) {
      return (rpcManager, pendingRpc) -> new StreamResponseFuture<>(rpcManager, pendingRpc, onNext);
    }

    private StreamResponseFuture(Endpoint endpoint, PendingRpc rpc, Consumer<ResponseT> onNext) {
      super(endpoint, rpc);
      this.onNext = onNext;
    }

    @Override
    public void doHandleNext(ResponseT value) {
      onNext.accept(value);
    }

    @Override
    public void doHandleCompleted() {
      future().set(status());
    }
  }
}
