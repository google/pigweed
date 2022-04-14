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
import java.util.Timer;
import java.util.TimerTask;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** Base class for tracking the state of a read or write transfer. */
abstract class Transfer<T> {
  private static final Logger logger = Logger.forClass(Transfer.class);

  interface ChunkSender {
    void send(Chunk chunk) throws TransferError;
  }

  /**
   * Transfers time out if the server doesn't send any chunks for a specified amount of time. The
   * timeout class detects when transfers have timed out and triggers a retry or aborts.
   */
  private class Timeout {
    private final int timeoutMillis;
    private final Timer timer;
    private TimerTask timeoutHandler = null;
    private boolean stopped = false;

    private Timeout(Timer timer, int timeoutMillis) {
      this.timeoutMillis = timeoutMillis;
      this.timer = timer;
    }

    private synchronized void schedule() {
      schedule(this.timeoutMillis);
    }

    private synchronized void schedule(int timeoutMillis) {
      // Cancel in case a previous instance of the timer is running. This could happen if a chunk
      // happened to arrive immediately after a transfer started, before the starting thread had a
      // chance to call schedule().
      cancel();
      timeoutHandler = new TimerTask() {
        private int retries = 0;

        @Override
        public void run() {
          if (stopped) {
            logger.atFiner().log(
                "Transfer %d stopped before timeout executed; not invoking retry", id);
            this.cancel();
            return;
          }

          if (retries++ < maxRetries) {
            logger.atFiner().log("Transfer %d received no chunks for %d ms; retrying %d/%d",
                id,
                timeoutMillis,
                retries,
                maxRetries);
            retryAfterTimeout();
          } else {
            this.cancel();
            sendFinalChunk(Status.DEADLINE_EXCEEDED);
          }
        }
      };
      // Execute the task every timeoutMillis until it is cancelled.
      timer.schedule(timeoutHandler, timeoutMillis, timeoutMillis);
    }

    private synchronized void cancel() {
      if (timeoutHandler != null) {
        timeoutHandler.cancel();
        timeoutHandler = null;
      }
    }

    private synchronized void stop() {
      cancel();
      stopped = true;
    }
  }

  private final int id;
  private final ChunkSender chunkSender;
  private final Consumer<Integer> unregisterTransfer;
  private final SettableFuture<T> future;
  private final Timeout timeout;
  private final int maxRetries;
  private final int initialTimeoutMillis;
  private final Consumer<TransferProgress> progressCallback;
  private final BooleanSupplier shouldAbortCallback;

  private Instant startTime;
  private boolean isCleanedUp = false;

  // The number of times this transfer has retried due to an RPC disconnection. Limit this to
  // maxRetries to prevent repeated crashes if reading to / writing from a particular transfer is
  // causing crashes.
  private int disconnectionRetries = 0;

  /**
   * Creates a new read or write transfer.
   *
   * @param id The ID of the resource to transfer.
   * @param chunkSender Interface to use to send a chunk.
   * @param unregisterTransfer Callback that unregisters a completed transfer.
   * @param timer Timer to use to schedule transfer timeouts.
   * @param timeoutMillis Maximum time to wait for a chunk from the server.
   * @param initialTimeoutMillis Maximum time to wait for a initial chunk from the server.
   * @param maxRetries Number of times to retry due to a timeout or RPC error.
   * @param progressCallback Called each time a packet is sent.
   * @param shouldAbortCallback BooleanSupplier that returns true if a transfer should be aborted.
   */
  Transfer(int id,
      ChunkSender chunkSender,
      Consumer<Integer> unregisterTransfer,
      Timer timer,
      int timeoutMillis,
      int initialTimeoutMillis,
      int maxRetries,
      Consumer<TransferProgress> progressCallback,
      BooleanSupplier shouldAbortCallback) {
    this.id = id;
    this.chunkSender = chunkSender;
    this.unregisterTransfer = unregisterTransfer;

    this.future = SettableFuture.create();
    // Add listener to make sure we won't missing to handle the future cancellation
    future.addListener(this::handleCancellation, directExecutor());
    this.timeout = new Timeout(timer, timeoutMillis);
    this.initialTimeoutMillis = initialTimeoutMillis;
    this.maxRetries = maxRetries;
    this.progressCallback = progressCallback;
    this.shouldAbortCallback = shouldAbortCallback;
  }

  final int getId() {
    return id;
  }

  final SettableFuture<T> getFuture() {
    return future;
  }

  final synchronized void start() {
    logger.atConfig().log(
        "Start transfer with time out parameters : default timeout %d ms, initial timeout %d ms, %d"
            + " max retires.",
        timeout.timeoutMillis,
        initialTimeoutMillis,
        maxRetries);
    startTime = Instant.now();
    if (sendChunk(getInitialChunk())) {
      timeout.schedule(
          initialTimeoutMillis); // Only start the timeout if sending the chunk succeeded
    }
  }

  /** Returns the chunk to send that starts the transfer. */
  abstract Chunk getInitialChunk();

  /** Processes an incoming chunk from the server. */
  final void handleChunk(Chunk chunk) {
    // This method is unsynchronized to enable concurrent chunk handling; derived Read/Write
    // transfers must ensure they support concurrent handleDataChunk() calls.
    timeout.cancel();
    if (handleCancellation()) {
      return;
    }

    if (chunk.hasStatus()) {
      Status status = Status.fromCode(chunk.getStatus());
      if (status != null) {
        cleanUp(status);
      } else {
        logger.atWarning().log("Received invalid status value %d", chunk.getStatus());
        sendFinalChunk(Status.INVALID_ARGUMENT);
      }
      return;
    }

    if (handleDataChunk(chunk)) {
      // Only schedule the timeout if the chunk was handled completely; if it was interrupted, the
      // interrupting thread will schedule the timeout.
      timeout.schedule();
    }
  }

  /** Restarts a transfer after an RPC disconnection. */
  final synchronized void handleDisconnection() {
    timeout.cancel();
    if (disconnectionRetries++ < maxRetries) {
      logger.atFine().log(
          "Restarting the pw_transfer RPC for transfer %d (attempt %d)", id, disconnectionRetries);
      if (sendChunk(getInitialChunk())) {
        timeout.schedule(); // Only start the timeout if sending the chunk succeeded
      }
    } else {
      cleanUp(new TransferError(
          "Transfer " + id + " restarted " + maxRetries + " times, aborting", Status.INTERNAL));
    }
  }

  /** Action to take to retry after a timeout occurs. Must handle concurrent invocation. */
  abstract void retryAfterTimeout();

  /**
   * Processes an incoming data chunk from the server. Only called for non-terminating chunks (i.e.
   * those without a status). Returns true if chunk was handled completely, and false if handling
   * the chunk was interrupted.
   */
  abstract boolean handleDataChunk(Chunk chunk);

  /** Sets the result for the future after a successful transfer. */
  abstract void setFutureResult();

  /**
   * Checks if the future was cancelled and aborts the transfer if it was.
   *
   * @return true if the transfer was cancelled
   */
  final synchronized boolean handleCancellation() {
    if (future.isCancelled()) {
      sendFinalChunk(Status.CANCELLED);
      return true;
    }
    return false;
  }

  final Chunk.Builder newChunk() {
    // TODO(frolv): Properly set the session ID after it is configured by the server.
    return Chunk.newBuilder().setSessionId(getId());
  }

  /** Sends a chunk. Returns true if sent, false if sending failed and the transfer was aborted. */
  final synchronized boolean sendChunk(Chunk chunk) {
    if (shouldAbortCallback.getAsBoolean()) {
      logger.atWarning().log("Abort signal received.");
      cleanUp(new TransferError(id, Status.ABORTED));
      return false;
    }

    try {
      chunkSender.send(chunk);
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
  private synchronized void cleanUp(Status status) {
    timeout.stop();
    unregisterTransfer.accept(id);

    logger.atInfo().log("Transfer %d completed with status %s", id, status);
    if (status.ok()) {
      setFutureResult();
    } else {
      future.setException(new TransferError(id, status));
    }
    isCleanedUp = true;
  }

  /** Finishes the transfer due to an exception. No packets are sent to the server. */
  final synchronized void cleanUp(TransferError exception) {
    timeout.stop();
    unregisterTransfer.accept(id);

    logger.atWarning().withCause(exception).log("Transfer %d terminated with exception", id);
    future.setException(exception);
    isCleanedUp = true;
  }

  /** Sends a status chunk to the server and finishes the transfer. */
  final synchronized void sendFinalChunk(Status status) {
    logger.atFine().log("Sending final chunk for transfer %d with status %s", id, status);
    // If the transfer was completed due to concurrently handling chunks, don't send.
    if (isCleanedUp) {
      logger.atFine().log("Skipping sending final chunk on already-completed transfer");
      return;
    }

    // Only call finish() if the sendChunk was successful. If it wasn't, the exception would have
    // already terminated the transfer.
    if (sendChunk(newChunk().setStatus(status.code()))) {
      cleanUp(status);
    }
  }

  /** Invokes the transfer progress callback and logs the progress. */
  final synchronized void updateProgress(
      long bytesSent, long bytesConfirmedReceived, long totalSizeBytes) {
    TransferProgress progress =
        TransferProgress.create(bytesSent, bytesConfirmedReceived, totalSizeBytes);
    progressCallback.accept(progress);

    long durationNanos = Duration.between(startTime, Instant.now()).toNanos();
    long totalRate = durationNanos == 0 ? 0 : (bytesSent * 1_000_000_000 / durationNanos);

    logger.atFine().log("Transfer %d progress: "
            + "%5.1f%% (%d B sent, %d B confirmed received of %s B total) at %d B/s",
        id,
        progress.percentReceived(),
        bytesSent,
        bytesConfirmedReceived,
        totalSizeBytes == UNKNOWN_TRANSFER_SIZE ? "unknown" : totalSizeBytes,
        totalRate);
  }
}
