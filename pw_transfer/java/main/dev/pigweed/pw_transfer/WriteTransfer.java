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

import static java.lang.Math.min;

import com.google.protobuf.ByteString;
import dev.pigweed.pw_log.Logger;
import dev.pigweed.pw_rpc.Status;
import dev.pigweed.pw_transfer.TransferEventHandler.TransferInterface;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

class WriteTransfer extends Transfer<Void> {
  private static final Logger logger = Logger.forClass(WriteTransfer.class);

  // Short chunk delays often turn into much longer delays. Ignore delays <10ms to avoid impacting
  // performance.
  private static final int MIN_CHUNK_DELAY_TO_SLEEP_MICROS = 10000;

  private int maxChunkSizeBytes = 0;
  private int minChunkDelayMicros = 0;
  private int sentOffset;
  private long totalDroppedBytes;
  private Chunk lastChunk;

  private final byte[] data;

  protected WriteTransfer(int resourceId,
      TransferInterface transferManager,
      int timeoutMillis,
      int initialTimeoutMillis,
      int maxRetries,
      byte[] data,
      Consumer<TransferProgress> progressCallback,
      BooleanSupplier shouldAbortCallback) {
    super(resourceId,
        transferManager,
        timeoutMillis,
        initialTimeoutMillis,
        maxRetries,
        progressCallback,
        shouldAbortCallback);
    this.data = data;
    this.lastChunk = newChunk(Chunk.Type.START)
                         .setResourceId(getSessionId())
                         .setRemainingBytes(data.length)
                         .build();
  }

  @Override
  State getWaitingForDataState() {
    return new WaitingForTransferParameters();
  }

  private class WaitingForTransferParameters extends State {
    @Override
    void handleTimeout() {
      Recovery recoveryState = new Recovery();
      setState(recoveryState);
      recoveryState.handleTimeout();
    }

    @Override
    void handleDataChunk(Chunk chunk) {
      updateTransferParameters(chunk);
    }
  }

  /** Transmitting a transfer window. */
  private class Transmitting extends State {
    private final int windowStartOffset;
    private final int windowEndOffset;

    Transmitting(int windowStartOffset, int windowEndOffset) {
      this.windowStartOffset = windowStartOffset;
      this.windowEndOffset = windowEndOffset;
    }

    @Override
    void handleDataChunk(Chunk chunk) {
      updateTransferParameters(chunk);
    }

    @Override
    void handleTimeout() {
      ByteString chunkData = ByteString.copyFrom(
          data, sentOffset, min(windowEndOffset - sentOffset, maxChunkSizeBytes));

      logger.atFiner().log("Transfer %d: sending bytes %d-%d (%d B chunk, max size %d B)",
          getSessionId(),
          sentOffset,
          sentOffset + chunkData.size() - 1,
          chunkData.size(),
          maxChunkSizeBytes);

      Chunk chunkToSend = buildDataChunk(chunkData);

      // If there's a timeout, resending this will trigger a transfer parameters update.
      lastChunk = chunkToSend;

      if (!sendChunk(chunkToSend)) {
        setState(new Completed());
        return;
      }

      sentOffset += chunkData.size();
      updateProgress(sentOffset, windowStartOffset, data.length);

      if (sentOffset < windowEndOffset) {
        setTimeoutMicros(minChunkDelayMicros);
        return; // Keep transmitting packets
      }
      setNextChunkTimeout();
      setState(new WaitingForTransferParameters());
    }
  }

  @Override
  Chunk getChunkForRetry() {
    // The service should resend transfer parameters if there was a timeout. In case the service
    // doesn't support timeouts and to avoid unnecessary waits, resend the last chunk. If there
    // were drops, this will trigger a transfer parameters update.
    return lastChunk;
  }

  @Override
  void setFutureResult() {
    updateProgress(data.length, data.length, data.length);
    getFuture().set(null);
  }

  private void updateTransferParameters(Chunk chunk) {
    logger.atFiner().log("Transfer %d received new chunk (type=%s, offset=%d, windowEndOffset=%d)",
        getSessionId(),
        chunk.getType(),
        chunk.getOffset(),
        chunk.getWindowEndOffset());

    if (chunk.getOffset() > data.length) {
      sendFinalChunk(Status.OUT_OF_RANGE);
      setState(new Completed());
      return;
    }

    int windowEndOffset = getWindowEndOffset(chunk, data.length);
    if (isRetransmit(chunk)) {
      long droppedBytes = sentOffset - chunk.getOffset();
      if (droppedBytes > 0) {
        totalDroppedBytes += droppedBytes;
        logger.atFine().log("Transfer %d retransmitting %d B (%d retransmitted of %d sent)",
            getSessionId(),
            droppedBytes,
            totalDroppedBytes,
            sentOffset);
      }
      sentOffset = (int) chunk.getOffset();
    } else if (windowEndOffset <= sentOffset) {
      logger.atFiner().log("Transfer %d: ignoring old rolling window packet", getSessionId());
      setNextChunkTimeout();
      return; // Received an old rolling window packet, ignore it.
    }

    // Update transfer parameters if they're set.
    if (chunk.hasMaxChunkSizeBytes()) {
      maxChunkSizeBytes = chunk.getMaxChunkSizeBytes();
    }
    if (chunk.hasMinDelayMicroseconds()) {
      if (chunk.getMinDelayMicroseconds() > MIN_CHUNK_DELAY_TO_SLEEP_MICROS) {
        minChunkDelayMicros = chunk.getMinDelayMicroseconds();
      }
    }

    if (maxChunkSizeBytes == 0) {
      if (windowEndOffset == sentOffset) {
        logger.atWarning().log(
            "Server requested 0 bytes in write transfer %d; aborting", getSessionId());
        sendFinalChunk(Status.INVALID_ARGUMENT);
        setState(new Completed());
        return;
      }
      // Default to sending the entire window if the max chunk size is not specified (or is 0).
      maxChunkSizeBytes = windowEndOffset - sentOffset;
    }

    Transmitting transmittingState = new Transmitting((int) chunk.getOffset(), windowEndOffset);
    setState(transmittingState);
    transmittingState.handleTimeout(); // Immediately send the first packet
  }

  private static boolean isRetransmit(Chunk chunk) {
    // Retransmit is the default behavior for older versions of the transfer protocol, which don't
    // have a type field.
    return !chunk.hasType()
        || (chunk.getType().equals(Chunk.Type.PARAMETERS_RETRANSMIT)
            || chunk.getType().equals(Chunk.Type.START));
  }

  private static int getWindowEndOffset(Chunk chunk, int dataLength) {
    if (isRetransmit(chunk)) {
      // A retransmit chunk may use an older version of the transfer protocol, in which the
      // window_end_offset field is not guaranteed to exist.
      return min((int) chunk.getOffset() + chunk.getPendingBytes(), dataLength);
    }
    return min(chunk.getWindowEndOffset(), dataLength);
  }

  private Chunk buildDataChunk(ByteString chunkData) {
    Chunk.Builder chunk = newChunk(Chunk.Type.DATA).setOffset(sentOffset).setData(chunkData);

    // If this is the last data chunk, setRemainingBytes to 0.
    if (sentOffset + chunkData.size() == data.length) {
      logger.atFiner().log(
          "Transfer %d sending final chunk with %d B", getSessionId(), chunkData.size());
      chunk.setRemainingBytes(0);
    }
    return chunk.build();
  }
}
