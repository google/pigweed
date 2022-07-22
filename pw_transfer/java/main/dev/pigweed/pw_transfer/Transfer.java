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

package dev.pigweed.pw_transfer;

import static com.google.common.util.concurrent.MoreExecutors.directExecutor;
import static dev.pigweed.pw_transfer.TransferProgress.UNKNOWN_TRANSFER_SIZE;

import com.google.common.util.concurrent.SettableFuture;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Status;
import java.time.Duration;
import java.time.Instant;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** Base class for tracking the state of a read or write transfer. */
abstract class Transfer<T> {
  private static final Logger logger = Logger.forClass(Transfer.class);

  // Largest nanosecond instant. Used to block indefinitely when no transfers are pending.
  static final Instant NO_TIMEOUT = Instant.ofEpochSecond(0, Long.MAX_VALUE);

  private final int resourceId;
  private final int sessionId;
  private final TransferEventHandler.TransferInterface eventHandler;
  private final SettableFuture<T> future;
  private final int maxRetries;
  private final int timeoutMillis;
  private final int initialTimeoutMillis;
  private final Consumer<TransferProgress> progressCallback;
  private final BooleanSupplier shouldAbortCallback;
  private final Instant startTime;

  private State state = new Inactive();
  private Instant deadline = NO_TIMEOUT;
  private boolean isCleanedUp = false;

  // The number of times this transfer has retried due to an RPC disconnection. Limit this to
  // maxRetries to prevent repeated crashes if reading to / writing from a particular transfer is
  // causing crashes.
  private int disconnectionRetries = 0;

  /**
   * Creates a new read or write transfer.
   * @param resourceId The resource ID of the transfer
   * @param eventHandler Interface to use to send a chunk.
   * @param timeoutMillis Maximum time to wait for a chunk from the server.
   * @param initialTimeoutMillis Maximum time to wait for the first chunk from the server.
   * @param maxRetries Number of times to retry due to a timeout or RPC error.
   * @param progressCallback Called each time a packet is sent.
   * @param shouldAbortCallback BooleanSupplier that returns true if a transfer should be aborted.
   */
  Transfer(int resourceId,
      TransferEventHandler.TransferInterface eventHandler,
      int timeoutMillis,
      int initialTimeoutMillis,
      int maxRetries,
      Consumer<TransferProgress> progressCallback,
      BooleanSupplier shouldAbortCallback) {
    this.resourceId = resourceId;
    this.sessionId = resourceId; // TODO(b/227211777): Distinguish between resource & session ID
    this.eventHandler = eventHandler;

    this.future = SettableFuture.create();
    this.timeoutMillis = timeoutMillis;
    this.initialTimeoutMillis = initialTimeoutMillis;
    this.maxRetries = maxRetries;
    this.progressCallback = progressCallback;
    this.shouldAbortCallback = shouldAbortCallback;

    // If the future is cancelled, tell the TransferEventHandler to cancel the transfer.
    future.addListener(() -> {
      if (future.isCancelled()) {
        eventHandler.cancelTransfer(this);
      }
    }, directExecutor());

    startTime = Instant.now();
  }

  final int getResourceId() {
    return resourceId;
  }

  final int getSessionId() {
    return sessionId;
  }

  final Instant getDeadline() {
    return deadline;
  }

  final void setNextChunkTimeout() {
    deadline = Instant.now().plusMillis(timeoutMillis);
  }

  private void setInitialTimeout() {
    deadline = Instant.now().plusMillis(initialTimeoutMillis);
  }

  final void setTimeoutMicros(int timeoutMicros) {
    deadline = Instant.now().plusNanos((long) timeoutMicros * 1000);
  }

  final SettableFuture<T> getFuture() {
    return future;
  }

  final void start() {
    logger.atFine().log(
        "Transfer %d for resource %d starting with parameters: default timeout %d ms, initial timeout %d ms, %d max retires",
        getSessionId(),
        getResourceId(),
        timeoutMillis,
        initialTimeoutMillis,
        maxRetries);
    if (!sendChunk(getChunkForRetry())) {
      return; // Sending failed, transfer is cancelled
    }

    setState(getWaitingForDataState());
    setInitialTimeout();
  }

  /** Processes an incoming chunk from the server. */
  final void handleChunk(Chunk chunk) {
    // Since a packet has been received, don't allow retries on disconnection; abort instead.
    disconnectionRetries = Integer.MAX_VALUE;

    if (chunk.hasStatus()) {
      logger.atFinest().log("Event: handle final chunk");
      state.handleFinalChunk(chunk);
    } else {
      logger.atFinest().log("Event: handle data chunk");
      state.handleDataChunk(chunk);
    }
  }

  final void handleTimeoutIfDeadlineExceeded() {
    if (Instant.now().isAfter(deadline)) {
      logger.atFinest().log("Event: handleTimeout since %s is after %s", Instant.now(), deadline);
      state.handleTimeout();
    }
  }

  final void handleTermination() {
    state.handleTermination();
  }

  final void handleCancellation() {
    state.handleCancellation();
  }

  /** Restarts a transfer after an RPC disconnection. */
  final void handleDisconnection() {
    // disconnectionRetries is set to Int.MAX_VALUE when a packet is received to prevent retries
    // after the initial packet.
    if (disconnectionRetries++ < maxRetries) {
      logger.atFine().log("Restarting the pw_transfer RPC for transfer %d (attempt %d/%d)",
          sessionId,
          disconnectionRetries,
          maxRetries);
      if (sendChunk(getChunkForRetry())) {
        setInitialTimeout();
      }
    } else {
      cleanUp(new TransferError(
          "Transfer " + sessionId + " restarted " + maxRetries + " times, aborting",
          Status.INTERNAL));
    }
  }

  /** Returns the State to enter immediately after sending the first packet. */
  abstract State getWaitingForDataState();

  /**
   * Returns the chunk to send for a retry. Returns the initial chunk if no chunks have been sent.
   */
  abstract Chunk getChunkForRetry();

  /** Sets the result for the future after a successful transfer. */
  abstract void setFutureResult();

  final Chunk.Builder newChunk(Chunk.Type type) {
    // TODO(frolv): Properly set the session ID after it is configured by the server.
    return Chunk.newBuilder().setTransferId(getSessionId()).setType(type);
  }

  final void setState(State newState) {
    if (newState != state) {
      logger.atFinest().log(
          "Transfer %d state %s -> %s", getSessionId(), state.getName(), newState.getName());
    }
    state = newState;
  }

  /** Sends a chunk. Returns true if sent, false if sending failed and the transfer was aborted. */
  final boolean sendChunk(Chunk chunk) {
    if (shouldAbortCallback.getAsBoolean()) {
      logger.atWarning().log("Abort signal received.");
      cleanUp(new TransferError(sessionId, Status.ABORTED));
      return false;
    }

    try {
      eventHandler.sendChunk(chunk);
      return true;
    } catch (TransferError transferError) {
      cleanUp(transferError);
      return false;
    }
  }

  final boolean sendChunk(Chunk.Builder chunk) {
    return sendChunk(chunk.build());
  }

  /** Performs final cleanup of a completed transfer. No packets are sent to the server. */
  private void cleanUp(Status status) {
    eventHandler.unregisterTransfer(sessionId);

    logger.atInfo().log("Transfer %d completed with status %s", sessionId, status);
    if (status.ok()) {
      setFutureResult();
    } else {
      future.setException(new TransferError(sessionId, status));
    }
    isCleanedUp = true;
  }

  /** Finishes the transfer due to an exception. No packets are sent to the server. */
  final void cleanUp(TransferError exception) {
    eventHandler.unregisterTransfer(sessionId);

    logger.atWarning().withCause(exception).log("Transfer %d terminated with exception", sessionId);
    future.setException(exception);
    isCleanedUp = true;
  }

  /** Sends a status chunk to the server and finishes the transfer. */
  final void sendFinalChunk(Status status) {
    logger.atFine().log("Sending final chunk for transfer %d with status %s", sessionId, status);
    // If the transfer was completed due to concurrently handling chunks, don't send.
    if (isCleanedUp) {
      logger.atFine().log("Skipping sending final chunk on already-completed transfer");
      return;
    }

    // Only call finish() if the sendChunk was successful. If it wasn't, the exception would have
    // already terminated the transfer.
    if (sendChunk(newChunk(Chunk.Type.COMPLETION).setStatus(status.code()))) {
      cleanUp(status);
    }
  }

  /** Invokes the transfer progress callback and logs the progress. */
  final void updateProgress(long bytesSent, long bytesConfirmedReceived, long totalSizeBytes) {
    TransferProgress progress =
        TransferProgress.create(bytesSent, bytesConfirmedReceived, totalSizeBytes);
    progressCallback.accept(progress);

    long durationNanos = Duration.between(startTime, Instant.now()).toNanos();
    long totalRate = durationNanos == 0 ? 0 : (bytesSent * 1_000_000_000 / durationNanos);

    logger.atFine().log("Transfer %d progress: "
            + "%5.1f%% (%d B sent, %d B confirmed received of %s B total) at %d B/s",
        sessionId,
        progress.percentReceived(),
        bytesSent,
        bytesConfirmedReceived,
        totalSizeBytes == UNKNOWN_TRANSFER_SIZE ? "unknown" : totalSizeBytes,
        totalRate);
  }

  /** Represents a state in the transfer state machine. */
  abstract class State {
    private String getName() {
      return getClass().getSimpleName();
    }

    /** Called to handle a non-final chunk for this transfer. */
    void handleDataChunk(Chunk chunk) {
      logger.atFine().log(
          "Transfer %d [%s state]: Received unexpected data chunk", getSessionId(), getName());
    }

    /** Called to handle the final chunk for this transfer. */
    void handleFinalChunk(Chunk chunk) {
      Status status = Status.fromCode(chunk.getStatus());
      if (status != null) {
        cleanUp(status);
      } else {
        logger.atWarning().log("Received invalid status value %d", chunk.getStatus());
        sendFinalChunk(Status.INVALID_ARGUMENT);
      }
      setState(new Completed());
    }

    /** Called when this transfer's deadline expires. */
    void handleTimeout() {
      logger.atFine().log("Transfer %d [%s state]: Ignoring timeout", getSessionId(), getName());
    }

    /** Called if the transfer is cancelled by the user. */
    void handleCancellation() {
      sendFinalChunk(Status.CANCELLED);
      setState(new Completed());
    }

    /** Called when the transfer thread is shutting down. */
    void handleTermination() {
      sendFinalChunk(Status.ABORTED);
      setState(new Completed());
    }
  }

  private class Inactive extends State {}

  /** Recovering from an expired timeout. */
  class Recovery extends State {
    private int retries;

    @Override
    void handleDataChunk(Chunk chunk) {
      setState(getWaitingForDataState());
      state.handleDataChunk(chunk);
    }

    @Override
    void handleTimeout() {
      if (retries < maxRetries) {
        logger.atFiner().log("Transfer %d received no chunks for %d ms; retrying %d/%d",
            getSessionId(),
            timeoutMillis,
            retries,
            maxRetries);
        sendChunk(getChunkForRetry());
        retries += 1;
        setNextChunkTimeout();
        return;
      }

      sendFinalChunk(Status.DEADLINE_EXCEEDED);
      setState(new Completed());
    }
  }

  /** Transfer completed. Do nothing if the transfer is terminated or cancelled. */
  class Completed extends State {
    Completed() {
      deadline = NO_TIMEOUT;
    }

    @Override
    void handleTermination() {}

    @Override
    void handleCancellation() {}
  }
}
