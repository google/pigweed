// Copyright 2024 The Pigweed Authors
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

#include "pw_uart_mcuxpresso/dma_uart_nonblocking.h"

#include <optional>

#include "fsl_common_arm.h"
#include "fsl_dma.h"
#include "fsl_flexcomm.h"
#include "fsl_usart_dma.h"
#include "pw_assert/check.h"
#include "pw_bytes/byte_builder.h"
#include "pw_log/log.h"
#include "pw_preprocessor/util.h"

namespace pw::uart {

// Deinitialize the DMA channels and USART.
void DmaUartMcuxpressoNonBlocking::Deinit() {
  if (!initialized_) {
    return;
  }

  config_.tx_dma_ch.Disable();
  config_.rx_dma_ch.Disable();

  USART_Deinit(config_.usart_base);
  clock_tree_element_controller_.Release().IgnoreError();
  initialized_ = false;
}

DmaUartMcuxpressoNonBlocking::~DmaUartMcuxpressoNonBlocking() { Deinit(); }

// Initialize the USART and DMA channels based on the configuration
// specified during object creation.
Status DmaUartMcuxpressoNonBlocking::Init() {
  if (config_.usart_base == nullptr) {
    return Status::InvalidArgument();
  }
  if (config_.baud_rate == 0) {
    return Status::InvalidArgument();
  }

  usart_config_t defconfig;
  USART_GetDefaultConfig(&defconfig);

  defconfig.baudRate_Bps = config_.baud_rate;
  defconfig.enableHardwareFlowControl = config_.flow_control;
  defconfig.parityMode = config_.parity;
  defconfig.enableTx = true;
  defconfig.enableRx = true;

  PW_TRY(clock_tree_element_controller_.Acquire());
  flexcomm_clock_freq_ =
      CLOCK_GetFlexcommClkFreq(FLEXCOMM_GetInstance(config_.usart_base));
  status_t status =
      USART_Init(config_.usart_base, &defconfig, flexcomm_clock_freq_);
  if (status != kStatus_Success) {
    clock_tree_element_controller_.Release().IgnoreError();
    return Status::Internal();
  }

  tx_data_.tx_idx = 0;

  rx_data_.data_received = 0;
  rx_data_.data_copied = 0;
  rx_data_.ring_buffer_read_idx = 0;
  rx_data_.ring_buffer_write_idx = 0;

  {
    // We need exclusive access to INPUTMUX registers, as it is used by many DMA
    // peripherals.
    std::lock_guard lock(interrupt_lock_);

    // Temporarily enable clock to inputmux, so that RX and TX DMA requests can
    // get enabled.
    INPUTMUX_Init(INPUTMUX);
    INPUTMUX_EnableSignal(
        INPUTMUX, config_.rx_input_mux_dmac_ch_request_en, true);
    INPUTMUX_EnableSignal(
        INPUTMUX, config_.tx_input_mux_dmac_ch_request_en, true);
    INPUTMUX_Deinit(INPUTMUX);
  }

  config_.tx_dma_ch.Enable();
  config_.rx_dma_ch.Enable();

  // Initialized enough for Deinit code to handle any errors from here.
  initialized_ = true;

  status = USART_TransferCreateHandleDMA(config_.usart_base,
                                         &uart_dma_handle_,
                                         DmaCallback,
                                         this,
                                         config_.tx_dma_ch.handle(),
                                         config_.rx_dma_ch.handle());

  if (status != kStatus_Success) {
    Deinit();
    return Status::Internal();
  }

  {
    std::lock_guard lock(interrupt_lock_);

    rx_data_.request.valid = false;
    tx_data_.request.valid = false;

    // Begin reading into the ring buffer.
    TriggerReadDmaIntoRingBuffer();
  }

  return OkStatus();
}

Status DmaUartMcuxpressoNonBlocking::DoEnable(bool enable) {
  if (enable == initialized_) {
    return OkStatus();
  }

  if (enable) {
    return Init();
  } else {
    Deinit();
    return OkStatus();
  }
}

// Trigger a RX DMA into the ring buffer.
// The ring buffer is the DMA target when there is NOT an active read request.
void DmaUartMcuxpressoNonBlocking::TriggerReadDmaIntoRingBuffer() {
  PW_DCHECK(!rx_data_.request.valid);

  rx_data_.target = DmaRxTarget::kRingBuffer;

  uint8_t* ring_buffer =
      reinterpret_cast<uint8_t*>(rx_data_.ring_buffer.data());
  rx_data_.transfer.data = &ring_buffer[rx_data_.ring_buffer_write_idx];

  // Are we about to run off the end of the ring buffer?
  if (rx_data_.ring_buffer_write_idx + kUsartDmaMaxTransferCount >
      rx_data_.ring_buffer.size_bytes()) {
    // Clamp the size of the transfer.
    rx_data_.transfer.dataSize =
        rx_data_.ring_buffer.size_bytes() - rx_data_.ring_buffer_write_idx;
  } else {
    // Otherwise, read the maximum number of bytes.
    rx_data_.transfer.dataSize = kUsartDmaMaxTransferCount;
  }

  PW_DCHECK_UINT_LE(rx_data_.ring_buffer_write_idx + rx_data_.transfer.dataSize,
                    rx_data_.ring_buffer.size_bytes());

  PW_LOG_DEBUG("TriggerReadDma(Ring) write_idx[%u-%u) size(%u)",
               rx_data_.ring_buffer_write_idx,
               rx_data_.ring_buffer_write_idx + rx_data_.transfer.dataSize,
               rx_data_.transfer.dataSize);

  USART_TransferReceiveDMA(
      config_.usart_base, &uart_dma_handle_, &rx_data_.transfer);
}

// Trigger a RX DMA into the user buffer.
// The user buffer is the DMA target when there is an active read request.
void DmaUartMcuxpressoNonBlocking::TriggerReadDmaIntoUserBuffer() {
  PW_DCHECK(rx_data_.request.valid);

  rx_data_.target = DmaRxTarget::kUserBuffer;

  uint8_t* user_buffer =
      reinterpret_cast<uint8_t*>(rx_data_.request.buffer.data());
  rx_data_.transfer.data = &user_buffer[rx_data_.request.write_idx];

  rx_data_.transfer.dataSize =
      std::min(rx_data_.request.bytes_remaining, kUsartDmaMaxTransferCount);

  PW_LOG_DEBUG("TriggerReadDma(User) write_idx[%u-%u) size(%u)",
               rx_data_.request.write_idx,
               rx_data_.request.write_idx + rx_data_.transfer.dataSize,
               rx_data_.transfer.dataSize);

  USART_TransferReceiveDMA(
      config_.usart_base, &uart_dma_handle_, &rx_data_.transfer);
}

// Trigger a TX DMA from the user's buffer.
void DmaUartMcuxpressoNonBlocking::TriggerWriteDma() {
  const uint8_t* tx_buffer =
      reinterpret_cast<const uint8_t*>(tx_data_.buffer.data());
  tx_data_.transfer.txData = &tx_buffer[tx_data_.tx_idx];

  // If this is the final DMA transaction, we need to clamp the number of
  // transfer bytes.
  size_t bytes_remaining = tx_data_.buffer.size() - tx_data_.tx_idx;
  tx_data_.transfer.dataSize =
      std::min(bytes_remaining, kUsartDmaMaxTransferCount);

  USART_TransferSendDMA(
      config_.usart_base, &uart_dma_handle_, &tx_data_.transfer);
}

// Clear the RX DMA idle interrupt flag and returns whether the flag was set.
// This function is based on fsl_dma.cc::DMA_IRQHandle().
bool DmaUartMcuxpressoNonBlocking::ClearRxDmaInterrupt() {
  const dma_handle_t* handle = uart_dma_handle_.rxDmaHandle;
  const uint8_t channel_index = DMA_CHANNEL_INDEX(base, handle->channel);
  const bool interrupt_enabled =
      ((DMA_COMMON_REG_GET(handle->base, handle->channel, INTENSET) &
        (1UL << channel_index)) != 0UL);
  const bool channel_a_flag =
      ((DMA_COMMON_REG_GET(handle->base, handle->channel, INTA) &
        (1UL << channel_index)) != 0UL);
  const bool channel_b_flag =
      ((DMA_COMMON_REG_GET(handle->base, handle->channel, INTB) &
        (1UL << channel_index)) != 0UL);

  if (interrupt_enabled) {
    if (channel_a_flag) {
      DMA_COMMON_REG_SET(
          handle->base, handle->channel, INTA, (1UL << channel_index));
    }

    if (channel_b_flag) {
      DMA_COMMON_REG_SET(
          handle->base, handle->channel, INTB, (1UL << channel_index));
    }
  }

  const bool int_was_set =
      interrupt_enabled && (channel_a_flag || channel_b_flag);
  return int_was_set;
}

Status DmaUartMcuxpressoNonBlocking::DoRead(
    ByteSpan rx_buffer,
    size_t min_bytes,
    Function<void(Status status, ConstByteSpan buffer)>&& callback) {
  size_t max_bytes = rx_buffer.size();
  if (min_bytes == 0 || max_bytes == 0 || min_bytes > max_bytes) {
    return Status::InvalidArgument();
  }

  // We must grab the interrupt lock before reading the `valid` flag to avoid
  // racing with `TxRxCompletionCallback()`.
  std::lock_guard lock(interrupt_lock_);

  if (rx_data_.request.valid) {
    return Status::Unavailable();
  }
  rx_data_.request.valid = true;

  // The user has requested at least `min_bytes`, but will take up to
  // `max_bytes`. Our strategy is to copy as much buffered data as we can right
  // now, up to `max_bytes`. We start by consuming bytes from the ring buffer.
  // After exhausting the ring buffer, we cancel the in-flight DMA early to get
  // any data the DMA transferred, but hasn't been accounted for.
  // Since the DMA transfer size is large, data sitting in the transfer buffer
  // could potentially be quite old. If we _still_ don't have enough, we setup
  // a DMA for the remaining amount, directly into the user's buffer.
  //
  // We can split this up into three scenarios:
  // 1. The ring buffer has enough data to immediately complete the request.
  // 2. We can complete the request only if we cancel the in-flight DMA.
  // 3. We don't have enough data. Consume any bytes we have and start a DMA
  // directly into the user's buffer.
  //
  // (Note, scenarios 1 and 2 both complete the read request within the
  // `DoRead()` call.)
  //
  // The code below handles these three scenarios, but slightly reorders things
  // for the sake of optimization.

  // First, if we know the ring buffer isn't going to be enough, get the
  // DMA abort out of the way for scenarios 2 and 3.
  bool dma_aborted = false;
  size_t bytes_in_ring_buffer = rx_data_.data_received - rx_data_.data_copied;

  PW_LOG_DEBUG("DoRead min_bytes(%u) ring_bytes(%u) read_idx(%u) write_idx(%u)",
               min_bytes,
               bytes_in_ring_buffer,
               rx_data_.ring_buffer_read_idx,
               rx_data_.ring_buffer_write_idx);

  if (bytes_in_ring_buffer < min_bytes) {
    // Cancel the DMA
    USART_TransferAbortReceiveDMA(config_.usart_base, &uart_dma_handle_);

    // Get the number of bytes that the transfer didn't fulfill.
    size_t bytes_remaining =
        DMA_GetRemainingBytes(uart_dma_handle_.rxDmaHandle->base,
                              uart_dma_handle_.rxDmaHandle->channel);
    size_t bytes_received = rx_data_.transfer.dataSize - bytes_remaining;

    if (bytes_remaining == 0) {
      // We raced a completed RX DMA.
      // If we exit the critical section without doing anything, the DMA idle
      // interrupt will fire and call `TxRxCompletionCallback()`.
      // Clear the interrupt flag to prevent the ISR from firing.
      // We'll manually handle the completion here instead.
      const bool int_was_set = ClearRxDmaInterrupt();
      PW_DCHECK(int_was_set);
      HandleCompletedRxIntoRingBuffer();
    } else {
      // Otherwise, the DMA was successfully cancelled, with partial data
      // written to the ring buffer. Manually fix up any ring buffer accounting.
      rx_data_.ring_buffer_write_idx += bytes_received;
      rx_data_.data_received += bytes_received;

      if (rx_data_.ring_buffer_write_idx >= rx_data_.ring_buffer.size_bytes()) {
        PW_LOG_DEBUG("ring_buffer_write_idx rolled over in DoRead");
        rx_data_.ring_buffer_write_idx = 0;
      }
    }

    bytes_in_ring_buffer += bytes_received;

    // Data from the cancelled transfer should now be accounted for.

    dma_aborted = true;
  }

  // Now that we've dealt with any accounting issues from DMA cancellation,
  // we know if the ring buffer contains enough data to complete the request
  // immediately.
  const size_t copy_size = std::min(max_bytes, bytes_in_ring_buffer);
  const bool request_completed_internally = copy_size >= min_bytes;

  // Before we start copying out of the ring buffer, let's start the next DMA.
  // We want to minimize the time spent without a DMA in-flight, as that risks
  // data loss.
  if (request_completed_internally) {
    // We're about to complete the request with data just from the ring buffer.
    rx_data_.request.valid = false;
    if (dma_aborted) {
      // If we cancelled the DMA to complete this request, kick off the next
      // one manually.
      TriggerReadDmaIntoRingBuffer();
    }
  } else {
    // We still need more data to complete the request.
    // Configure the next DMA to point directly into the user buffer.
    // Note we can only request enough for `min_bytes`. If the user calls
    // Read() in a tight loop with `min_bytes = 1`, this can result in many
    // single-byte DMA transactions.
    rx_data_.request.buffer = rx_buffer;
    rx_data_.request.write_idx = copy_size;
    rx_data_.request.bytes_remaining = min_bytes - copy_size;
    rx_data_.request.bytes_requested = min_bytes;
    rx_data_.request.callback = std::move(callback);
    rx_data_.request.valid = true;
    TriggerReadDmaIntoUserBuffer();
  }

  // Copy all the data we can from the ring buffer.
  // This is needed in all three scenarios.
  if (copy_size > 0) {
    ByteSpan ring_buffer = rx_data_.ring_buffer;
    auto bb = ByteBuilder{rx_buffer};
    PW_LOG_DEBUG(
        "copy (%u bytes) @ [%u]", copy_size, rx_data_.ring_buffer_read_idx);

    // If the data crosses the end of the ring_buffer, we need to do a split
    // copy operation.
    if (rx_data_.ring_buffer_read_idx + copy_size > ring_buffer.size_bytes()) {
      // The first copy is any bytes at the end of the buffer.
      size_t first_copy_size =
          ring_buffer.size_bytes() - rx_data_.ring_buffer_read_idx;
      bb.append(
          ring_buffer.subspan(rx_data_.ring_buffer_read_idx, first_copy_size));

      // The second copy starts back at offset 0.
      size_t second_copy_size = copy_size - first_copy_size;
      bb.append(ring_buffer.subspan(0, second_copy_size));
      rx_data_.ring_buffer_read_idx = second_copy_size;

      PW_LOG_DEBUG("split copy first(%u bytes) second(%u bytes)",
                   first_copy_size,
                   second_copy_size);
    } else {
      // Otherwise, it's just a normal copy.
      PW_DCHECK_UINT_LE(rx_data_.ring_buffer_read_idx + copy_size,
                        ring_buffer.size_bytes());
      bb.append(ring_buffer.subspan(rx_data_.ring_buffer_read_idx, copy_size));
      rx_data_.ring_buffer_read_idx += copy_size;

      // But we still need to do the ring buffer accounting for our reader
      // index!
      if (rx_data_.ring_buffer_read_idx == ring_buffer.size_bytes()) {
        PW_LOG_DEBUG("read_idx rollover to 0 in DoRead");
        rx_data_.ring_buffer_read_idx = 0;
      }
    }

    // TODO(shined): `ring_buffer_read_idx` could be deleted entirely in a
    // refactor, since `data_copied` encodes the same information (if the
    // ring_buffer size is aligned.)
    rx_data_.data_copied += copy_size;
  }

  // Now that we've copied data out of the ring buffer, we can call the user
  // callback if the request has been completed.
  if (request_completed_internally) {
    PW_LOG_DEBUG("request completed in DoRead");
    callback(OkStatus(), rx_data_.request.buffer.first(copy_size));
  }

  return OkStatus();
}

Status DmaUartMcuxpressoNonBlocking::DoWrite(
    ConstByteSpan tx_buffer, Function<void(StatusWithSize status)>&& callback) {
  if (tx_buffer.size() == 0) {
    return Status::InvalidArgument();
  }

  PW_LOG_DEBUG("DoWrite: size(%u)", tx_buffer.size());

  std::lock_guard lock(interrupt_lock_);

  if (tx_data_.request.valid) {
    return Status::Unavailable();
  }
  tx_data_.request.valid = true;

  tx_data_.buffer = tx_buffer;
  tx_data_.tx_idx = 0;
  tx_data_.request.callback = std::move(callback);

  // Start the DMA. If multiple DMA transactions are needed, the completion
  // callback will set up subsequent transactions.
  TriggerWriteDma();

  return OkStatus();
}

// Static wrapper method called by the DMA completion ISR.
void DmaUartMcuxpressoNonBlocking::DmaCallback(USART_Type* base,
                                               usart_dma_handle_t* handle,
                                               status_t dma_status,
                                               void* userdata) {
  auto* uart = static_cast<DmaUartMcuxpressoNonBlocking*>(userdata);
  PW_CHECK_PTR_EQ(base, uart->config_.usart_base);
  PW_CHECK_PTR_EQ(handle, &uart->uart_dma_handle_);

  return uart->TxRxCompletionCallback(dma_status);
}

// This helper function is called by `TxRxCompletionCallback()` after a DMA
// transaction into the user buffer.
void DmaUartMcuxpressoNonBlocking::HandleCompletedRxIntoUserBuffer() {
  rx_data_.request.bytes_remaining -= rx_data_.transfer.dataSize;
  rx_data_.request.write_idx += rx_data_.transfer.dataSize;

  // Have we completed the read request?
  if (rx_data_.request.bytes_remaining == 0) {
    // Call the user's completion callback and invalidate the request.
    PW_LOG_DEBUG("request completed in callback");
    rx_data_.request.callback(
        OkStatus(),
        rx_data_.request.buffer.first(rx_data_.request.bytes_requested));
    rx_data_.request.valid = false;
  }
}

// This helper function is called by `TxRxCompletionCallback()` after a DMA
// transaction into the ring buffer. Additionally, it is called by `DoRead()`
// and `DoCancelRead()` after cancelling a DMA transaction _only_ if the
// cancellation raced with DMA completion.
void DmaUartMcuxpressoNonBlocking::HandleCompletedRxIntoRingBuffer() {
  rx_data_.ring_buffer_write_idx += rx_data_.transfer.dataSize;
  rx_data_.data_received += rx_data_.transfer.dataSize;

  PW_DCHECK_UINT_LE(rx_data_.data_received - rx_data_.data_copied,
                    rx_data_.ring_buffer.size_bytes());
  PW_DCHECK_UINT_LE(rx_data_.ring_buffer_write_idx,
                    rx_data_.ring_buffer.size_bytes());
  if (rx_data_.ring_buffer_write_idx == rx_data_.ring_buffer.size_bytes()) {
    PW_LOG_DEBUG("ring_buffer_write_idx rolled over in callback");
    rx_data_.ring_buffer_write_idx = 0;
  }
}

// This callback is called by both the RX and TX interrupt handlers.
// It is called upon completion of a DMA transaction.
void DmaUartMcuxpressoNonBlocking::TxRxCompletionCallback(status_t status) {
  std::lock_guard lock(interrupt_lock_);

  if (status == kStatus_USART_RxIdle) {
    // RX transaction complete

    // Was this DMA targeting the user buffer or the ring buffer?
    if (rx_data_.target == DmaRxTarget::kUserBuffer) {
      // We're DMA-ing directly into the user's buffer.
      HandleCompletedRxIntoUserBuffer();
    } else {
      // We're DMA-ing into the ring buffer.
      HandleCompletedRxIntoRingBuffer();
    }

    // Trigger the next DMA into either the user buffer or ring buffer,
    // depending on whether we are servicing a user request or not.
    if (rx_data_.request.valid) {
      TriggerReadDmaIntoUserBuffer();
    } else {
      TriggerReadDmaIntoRingBuffer();
    }
  } else if (status == kStatus_USART_TxIdle && tx_data_.request.valid) {
    // TX transaction complete
    // This codepath runs only when there is a valid TX request, as writes only
    // come from the user.
    tx_data_.tx_idx += tx_data_.transfer.dataSize;
    // Is the request complete?
    if (tx_data_.tx_idx == tx_data_.buffer.size_bytes()) {
      tx_data_.request.callback(StatusWithSize(tx_data_.buffer.size_bytes()));
      tx_data_.request.valid = false;
    } else {
      // No, set up a followup DMA.
      PW_CHECK_INT_LT(tx_data_.tx_idx, tx_data_.buffer.size_bytes());
      TriggerWriteDma();
    }
  }
}

bool DmaUartMcuxpressoNonBlocking::DoCancelRead() {
  std::lock_guard lock(interrupt_lock_);

  if (!rx_data_.request.valid) {
    return false;
  }

  // There _must_ be an RX DMA directly targeting the user buffer.
  // We know this because we are in a critical section and the request is valid.

  // Cancel the in-flight DMA.
  USART_TransferAbortReceiveDMA(config_.usart_base, &uart_dma_handle_);

  // Get the number of bytes the DMA transaction was short by.
  size_t dma_bytes_remaining =
      DMA_GetRemainingBytes(uart_dma_handle_.rxDmaHandle->base,
                            uart_dma_handle_.rxDmaHandle->channel);

  if (dma_bytes_remaining == 0) {
    // We raced a completed RX DMA.
    // If we exit the critical section without doing anything, the DMA idle
    // interrupt will fire and call `TxRxCompletionCallback()`.
    if (rx_data_.request.bytes_remaining == 0) {
      // ...and that DMA also completed the user's request.
      // Fail the cancellation; `TxRxCompletionCallback()` will complete the
      // transaction and call the user callback.
      // This is the only scenario where the read request could already be
      // complete.
      // This race is why we must use `DMA_GetRemainingBytes()` instead of
      // `USART_TransferGetReceiveCountDMA()`.
      return false;
    } else {
      // ...but that DMA was not enough to complete the request.
      // Clear the interrupt flag to prevent the ISR from firing.
      // We'll manually handle the completion here instead.
      const bool int_was_set = ClearRxDmaInterrupt();
      PW_DCHECK(int_was_set);
      HandleCompletedRxIntoRingBuffer();
    }
  } else {
    // We successfully cancelled an in-flight DMA transaction.
    // Account for the final bytes that got copied into the user buffer.
    rx_data_.request.bytes_remaining -=
        rx_data_.transfer.dataSize - dma_bytes_remaining;
  }

  size_t bytes_copied =
      rx_data_.request.bytes_requested - rx_data_.request.bytes_remaining;
  rx_data_.request.callback(Status::Cancelled(),
                            rx_data_.request.buffer.first(bytes_copied));
  rx_data_.request.valid = false;

  // Set up a new RX DMA into the ring buffer.
  TriggerReadDmaIntoRingBuffer();

  return true;
}

bool DmaUartMcuxpressoNonBlocking::DoCancelWrite() {
  std::lock_guard lock(interrupt_lock_);

  if (!tx_data_.request.valid) {
    return false;
  }

  // There is a TX DMA in-flight.
  // We know this because we are in a critical section and the request is valid.

  // Cancel the in-flight DMA.
  USART_TransferAbortSendDMA(config_.usart_base, &uart_dma_handle_);

  // Get the number of bytes the DMA transaction was short by.
  size_t dma_bytes_remaining =
      DMA_GetRemainingBytes(uart_dma_handle_.txDmaHandle->base,
                            uart_dma_handle_.txDmaHandle->channel);

  if (dma_bytes_remaining == 0 &&
      tx_data_.tx_idx + tx_data_.transfer.dataSize == tx_data_.buffer.size()) {
    // We raced a completed TX DMA, and that DMA completed the user's request.
    // The interrupt will fire once we exit this critical section.
    // Fail the cancellation; the TxRxCompletionCallback will complete the
    // transaction and call the user callback.
    // This is the only scenario where the write request could already be
    // complete.
    // This race is why we must use `DMA_GetRemainingBytes()` instead of
    // `USART_TransferGetSendCountDMA()`.
    return false;
  }

  size_t bytes_transmitted =
      tx_data_.tx_idx + (tx_data_.transfer.dataSize - dma_bytes_remaining);
  tx_data_.request.callback(StatusWithSize::Cancelled(bytes_transmitted));
  tx_data_.request.valid = false;

  return true;
}

size_t DmaUartMcuxpressoNonBlocking::DoConservativeReadAvailable() {
  std::lock_guard lock(interrupt_lock_);

  size_t bytes_received = rx_data_.data_received - rx_data_.data_copied;

  uint32_t count = 0;
  status_t status = USART_TransferGetReceiveCountDMA(
      config_.usart_base, &uart_dma_handle_, &count);
  if (status == kStatus_Success) {
    bytes_received += count;
  }

  return count;
}

Status DmaUartMcuxpressoNonBlocking::DoClearPendingReceiveBytes() {
  std::lock_guard lock(interrupt_lock_);

  if (rx_data_.request.valid) {
    // It doesn't make sense to clear the receive buffer when a read request
    // is in flight.
    return Status::FailedPrecondition();
  }

  // Note: This only clears the ring buffer, not any bytes from the current
  // DMA transaction. Those bytes could be quite old, and this function could
  // be improved to also cancel the in-flight RX transfer.
  size_t bytes_pending = rx_data_.data_received - rx_data_.data_copied;
  rx_data_.ring_buffer_read_idx += bytes_pending;
  rx_data_.ring_buffer_read_idx %= rx_data_.ring_buffer.size();
  rx_data_.data_copied = rx_data_.data_received;

  return OkStatus();
}

Status DmaUartMcuxpressoNonBlocking::DoSetBaudRate(uint32_t baud_rate) {
  if (baud_rate == 0) {
    return Status::InvalidArgument();
  }

  config_.baud_rate = baud_rate;

  if (!initialized_) {
    return OkStatus();
  }

  status_t status = USART_SetBaudRate(
      config_.usart_base, config_.baud_rate, flexcomm_clock_freq_);
  switch (status) {
    default:
      return Status::Unknown();
    case kStatus_USART_BaudrateNotSupport:
    case kStatus_InvalidArgument:
      return Status::InvalidArgument();
    case kStatus_Success:
      return OkStatus();
  }
}

Status DmaUartMcuxpressoNonBlocking::DoSetFlowControl(bool enable) {
  config_.flow_control = enable;

  if (initialized_) {
    USART_EnableCTS(config_.usart_base, enable);
  }

  return OkStatus();
}

// Unimplemented
Status DmaUartMcuxpressoNonBlocking::DoFlushOutput(
    Function<void(Status status)>&& /*callback*/) {
  return Status::Unimplemented();
}

// Unimplemented
bool DmaUartMcuxpressoNonBlocking::DoCancelFlushOutput() { return false; }

}  // namespace pw::uart
