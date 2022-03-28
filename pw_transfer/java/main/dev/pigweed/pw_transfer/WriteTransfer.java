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

import static java.lang.Math.max;
import static java.lang.Math.min;
import static java.util.concurrent.TimeUnit.MICROSECONDS;

import com.google.protobuf.ByteString;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Status;
import dev.pigweed.pw_transfer.Manager.ChunkSizeAdjuster;
import java.util.Timer;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

class WriteTransfer extends Transfer<Void> {
  private static final Logger logger = Logger.forClass(WriteTransfer.class);

  // Short chunk delays often turn into much longer delays. Ignore delays <10ms to avoid impacting
  // performance.
  private static final int MIN_CHUNK_DELAY_TO_SLEEP_MICROS = 10000;

  private final AtomicInteger receivedOffset = new AtomicInteger();

  private int maxChunkSizeBytes = 0;
  private int minChunkDelayMicros = 0;
  private int sentOffset;
  private long totalDroppedBytes;
  private Chunk lastChunk;

  private final byte[] data;
  private final ChunkSizeAdjuster chunkSizeAdjustment;

  protected WriteTransfer(int id,
      ChunkSender sendChunk,
      Consumer<Integer> endTransfer,
      Timer timer,
      int timeoutMillis,
      int initialTimeoutMillis,
      int maxRetries,
      byte[] data,
      Consumer<TransferProgress> progressCallback,
      BooleanSupplier shouldAbortCallback,
      ChunkSizeAdjuster chunkSizeAdjustment) {
    super(id,
        sendChunk,
        endTransfer,
        timer,
        timeoutMillis,
        initialTimeoutMillis,
        maxRetries,
        progressCallback,
        shouldAbortCallback);
    this.data = data;
    this.chunkSizeAdjustment = chunkSizeAdjustment;

    this.lastChunk = getInitialChunk();
  }

  @Override
  synchronized Chunk getInitialChunk() {
    return newChunk().setResourceId(getId()).setRemainingBytes(data.length).build();
  }

  @Override
  boolean handleDataChunk(Chunk chunk) {
    // Track the window count and abandon this window if another transfer parameters update arrives
    // while handling the current one. This outer method is not synchronized to enable the more
    // recent thread to coordinate without blocking via the atomic int. Then any other concurrently
    // executing handleChunk() instances can bail once they finish with their current chunk.
    logger.atFiner().log("Transfer %d received new chunk (type=%s, offset=%d, windowEndOffset=%d)",
        getId(),
        chunk.getType(),
        chunk.getOffset(),
        chunk.getWindowEndOffset());

    // The chunk's offset is always the largest offset received by the device.
    // Record the largest offset seen to avoid backtracking if a stale retransmit packet arrives.
    receivedOffset.updateAndGet(currentValue -> max(currentValue, (int) chunk.getOffset()));
    if (windowIsStale(chunk)) {
      return false;
    }

    return doHandleDataChunk(chunk);
  }

  private synchronized boolean doHandleDataChunk(Chunk chunk) {
    if (chunk.getOffset() > data.length) {
      sendFinalChunk(Status.OUT_OF_RANGE);
      return true;
    }

    // Check if a newer chunk has arrived while this thread waited to acquire the lock.
    if (windowIsStale(chunk)) {
      return false;
    }

    int windowEndOffset = getWindowEndOffset(chunk, data.length);
    if (isRetransmit(chunk)) {
      long droppedBytes = sentOffset - chunk.getOffset();
      if (droppedBytes > 0) {
        totalDroppedBytes += droppedBytes;
        logger.atFine().log("Transfer %d retransmitting %d B (%d retransmitted of %d sent)",
            getId(),
            droppedBytes,
            totalDroppedBytes,
            sentOffset);
      }
      sentOffset = (int) chunk.getOffset();
    } else if (windowEndOffset <= sentOffset) {
      logger.atFiner().log("Transfer %d: ignoring old rolling window packet", getId());
      return true; // Received an old rolling window packet, ignore it.
    }

    // Update transfer parameters if they're set.
    if (chunk.hasMaxChunkSizeBytes()) {
      maxChunkSizeBytes = chunk.getMaxChunkSizeBytes();
    }
    if (chunk.hasMinDelayMicroseconds()) {
      minChunkDelayMicros = chunk.getMinDelayMicroseconds();
    }

    if (maxChunkSizeBytes == 0) {
      if (windowEndOffset == sentOffset) {
        logger.atWarning().log("Server requested 0 bytes in write transfer %d; aborting", getId());
        sendFinalChunk(Status.INVALID_ARGUMENT);
        return true;
      }
      // Default to sending the entire window if the max chunk size is not specified (or is 0).
      maxChunkSizeBytes = windowEndOffset - sentOffset;
    }

    Chunk chunkToSend;
    do {
      // Pause for the minimum delay, if requested by the server.
      if (minChunkDelayMicros > MIN_CHUNK_DELAY_TO_SLEEP_MICROS) {
        try {
          MICROSECONDS.sleep(minChunkDelayMicros);
        } catch (InterruptedException e) {
          // Ignore the exception. It shouldn't matter if this is interrupted.
        }
      }

      if (handleCancellation()) {
        return true;
      }

      if (windowIsStale(chunk)) {
        // The false return prevents the surrounding transfer from scheduling the timeout, which
        // avoids unnecessary timeout scheduling. The interrupting thread/chunk will schedule a
        // timeout instead.
        return false;
      }

      ByteString chunkData = ByteString.copyFrom(
          data, sentOffset, min(windowEndOffset - sentOffset, maxChunkSizeBytes));

      // Apply the chunk size adjustment. The returned chunk size is capped at chunkData.size().
      // Sending 0 bytes of an non-empty chunk is invalid and aborts the transfer.
      final int newChunkSize = min(
          chunkSizeAdjustment.getAdjustedChunkSize(chunkData, maxChunkSizeBytes), chunkData.size());
      if ((!chunkData.isEmpty() && newChunkSize == 0) || newChunkSize < 0) {
        logger.atWarning().log(
            "Transfer %d: attempted to adjust %d B chunk to %d B; aborting transfer",
            getId(),
            chunkData.size(),
            newChunkSize);
        sendFinalChunk(Status.INVALID_ARGUMENT);
        return true;
      }
      logger.atFiner().log(
          "Transfer %d: sending %d B chunk (adjusted from %d B) with max size of %d",
          getId(),
          newChunkSize,
          chunkData.size(),
          maxChunkSizeBytes);

      chunkData = chunkData.substring(0, newChunkSize);

      chunkToSend = buildChunk(chunkData);

      // If there's a timeout, resending this will trigger a transfer parameters update.
      lastChunk = chunkToSend;

      if (!sendChunk(chunkToSend)) {
        return true;
      }

      sentOffset += chunkData.size();
      updateProgress(sentOffset, chunk.getOffset(), data.length);
    } while (sentOffset < windowEndOffset);
    return true;
  }

  private boolean windowIsStale(Chunk chunk) {
    int newReceivedOffset = receivedOffset.get();
    if (chunk.getOffset() < newReceivedOffset) {
      logger.atFine().log(
          "Transfer %d: abandoning stale write window (old offset %d, new offset %d)",
          getId(),
          chunk.getOffset(),
          newReceivedOffset);
      return true;
    }
    return false;
  }

  @Override
  void retryAfterTimeout() {
    // The service should resend transfer parameters if there was a timeout. In case the service
    // doesn't support timeouts and to avoid unnecessary waits, resend the last chunk. If there
    // were drops, this will trigger a transfer parameters update.
    sendChunk(lastChunk);
  }

  @Override
  void setFutureResult() {
    updateProgress(data.length, data.length, data.length);
    getFuture().set(null);
  }

  private static boolean isRetransmit(Chunk chunk) {
    // Retransmit is the default behavior for older versions of the transfer protocol, which don't
    // have a type field.
    return !chunk.hasType() || chunk.getType().equals(Chunk.Type.PARAMETERS_RETRANSMIT);
  }

  private static int getWindowEndOffset(Chunk chunk, int dataLength) {
    if (isRetransmit(chunk)) {
      // A retransmit chunk may use an older version of the transfer protocol, in which the
      // window_end_offset field is not guaranteed to exist.
      return min((int) chunk.getOffset() + chunk.getPendingBytes(), dataLength);
    }
    return min(chunk.getWindowEndOffset(), dataLength);
  }

  private Chunk buildChunk(ByteString chunkData) {
    Chunk.Builder chunk = newChunk().setOffset(sentOffset).setData(chunkData);

    // If this is the last data chunk, setRemainingBytes to 0.
    if (sentOffset + chunkData.size() == data.length) {
      logger.atFiner().log("Transfer %d sending final chunk with %d B", getId(), chunkData.size());
      chunk.setRemainingBytes(0);
    }
    return chunk.build();
  }
}
