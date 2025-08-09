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

#include "pw_uart_mcuxpresso/dma_uart.h"

#include <optional>

#include "fsl_flexcomm.h"
#include "pw_assert/check.h"
#include "pw_preprocessor/util.h"

namespace pw::uart {

// Deinitialize the DMA channels and USART.
void DmaUartMcuxpresso::Deinit() {
  if (!initialized_) {
    return;
  }

  USART_TransferAbortReceiveDMA(config_.usart_base, &uart_dma_handle_);

  config_.tx_dma_ch.Disable();
  config_.rx_dma_ch.Disable();

  USART_Deinit(config_.usart_base);
  clock_tree_element_.Release().IgnoreError();

  tx_data_.notification.release();
  rx_data_.notification.release();

  initialized_ = false;
}

DmaUartMcuxpresso::~DmaUartMcuxpresso() { Deinit(); }

// Initialize the USART and DMA channels based on the configuration
// specified during object creation.
Status DmaUartMcuxpresso::Init() {
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

  PW_TRY(clock_tree_element_.Acquire());
  flexcomm_clock_freq_ =
      CLOCK_GetFlexcommClkFreq(FLEXCOMM_GetInstance(config_.usart_base));
  status_t status =
      USART_Init(config_.usart_base, &defconfig, flexcomm_clock_freq_);
  if (status != kStatus_Success) {
    clock_tree_element_.Release().IgnoreError();
    return Status::Internal();
  }

  // We need to touch register space that can be shared
  // among several DMA peripherals, hence we need to access
  // it exclusively. We achieve exclusive access on non-SMP systems as
  // a side effect of acquiring the interrupt_lock_, since acquiring the
  // interrupt_lock_ disables interrupts on the current CPU, which means
  // we cannot get descheduled until we release the interrupt_lock_.
  interrupt_lock_.lock();

  // Temporarily enable clock to inputmux, so that RX and TX DMA requests can
  // get enabled.
  INPUTMUX_Init(INPUTMUX);
  INPUTMUX_EnableSignal(
      INPUTMUX, config_.rx_input_mux_dmac_ch_request_en, true);
  INPUTMUX_EnableSignal(
      INPUTMUX, config_.tx_input_mux_dmac_ch_request_en, true);
  INPUTMUX_Deinit(INPUTMUX);

  interrupt_lock_.unlock();

  config_.tx_dma_ch.Enable();
  config_.rx_dma_ch.Enable();

  tx_data_.Init();
  rx_data_.Init();

  // Initialized enough for Deinit code to handle any errors from here.
  initialized_ = true;

  status = USART_TransferCreateHandleDMA(config_.usart_base,
                                         &uart_dma_handle_,
                                         TxRxCompletionCallback,
                                         this,
                                         config_.tx_dma_ch.handle(),
                                         config_.rx_dma_ch.handle());

  if (status != kStatus_Success) {
    Deinit();
    return Status::Internal();
  }

  // Read into the rx ring buffer.
  interrupt_lock_.lock();
  TriggerReadDma();
  interrupt_lock_.unlock();

  return OkStatus();
}

void DmaUartMcuxpresso::UsartDmaTxData::Init() { tx_idx = 0; }

void DmaUartMcuxpresso::UsartDmaRxData::Init() {
  ring_buffer_read_idx = 0;
  ring_buffer_write_idx = 0;
  data_received = 0;
  data_copied = 0;
  completion_size = 0;
}

// DMA usart data into ring buffer
//
// At most kUsartDmaMaxTransferCount bytes can be copied per DMA transfer.
// If completion_size is specified and dataSize is larger than completion_size,
// the dataSize will be limited to completion_size so that the completion
// callback will be called once completion_size bytes have been received.
void DmaUartMcuxpresso::TriggerReadDma() {
  uint8_t* ring_buffer =
      reinterpret_cast<uint8_t*>(rx_data_.ring_buffer.data());
  rx_data_.transfer.data = &ring_buffer[rx_data_.ring_buffer_write_idx];

  if (rx_data_.ring_buffer_write_idx + kUsartDmaMaxTransferCount >
      rx_data_.ring_buffer.size_bytes()) {
    rx_data_.transfer.dataSize =
        rx_data_.ring_buffer.size_bytes() - rx_data_.ring_buffer_write_idx;
  } else {
    rx_data_.transfer.dataSize = kUsartDmaMaxTransferCount;
  }

  if (rx_data_.completion_size > 0 &&
      rx_data_.transfer.dataSize > rx_data_.completion_size) {
    // Completion callback will be called once this transfer completes.
    rx_data_.transfer.dataSize = rx_data_.completion_size;
  }

  USART_TransferReceiveDMA(
      config_.usart_base, &uart_dma_handle_, &rx_data_.transfer);
}

// DMA send buffer data
void DmaUartMcuxpresso::TriggerWriteDma() {
  const uint8_t* tx_buffer =
      reinterpret_cast<const uint8_t*>(tx_data_.buffer.data());
  tx_data_.transfer.txData = &tx_buffer[tx_data_.tx_idx];
  if (tx_data_.tx_idx + kUsartDmaMaxTransferCount >
      tx_data_.buffer.size_bytes()) {
    // Completion callback will be called once this transfer completes.
    tx_data_.transfer.dataSize = tx_data_.buffer.size_bytes() - tx_data_.tx_idx;
  } else {
    tx_data_.transfer.dataSize = kUsartDmaMaxTransferCount;
  }

  USART_TransferSendDMA(
      config_.usart_base, &uart_dma_handle_, &tx_data_.transfer);
}

// Completion callback for TX and RX transactions
void DmaUartMcuxpresso::TxRxCompletionCallback(USART_Type* /* base */,
                                               usart_dma_handle_t* /* state */,
                                               status_t status,
                                               void* param) {
  DmaUartMcuxpresso* stream = reinterpret_cast<DmaUartMcuxpresso*>(param);

  if (status == kStatus_USART_RxIdle) {
    // RX transfer

    // Acquire the interrupt_lock_ to ensure that on SMP systems
    // access to the rx_data is synchronized.
    stream->interrupt_lock_.lock();

    struct UsartDmaRxData* rx_data = &stream->rx_data_;
    rx_data->ring_buffer_write_idx += rx_data->transfer.dataSize;
    rx_data->data_received += rx_data->transfer.dataSize;

    PW_DCHECK_INT_LE(rx_data->ring_buffer_write_idx,
                     rx_data->ring_buffer.size_bytes());
    if (rx_data->ring_buffer_write_idx == rx_data->ring_buffer.size_bytes()) {
      rx_data->ring_buffer_write_idx = 0;
    }

    bool notify_rx_completion = false;
    if (rx_data->completion_size > 0) {
      PW_DCHECK_INT_GE(rx_data->completion_size, rx_data->transfer.dataSize);
      rx_data->completion_size -= rx_data->transfer.dataSize;
      if (rx_data->completion_size == 0) {
        // We have satisfied the receive request, we must wake up the receiver.
        // Before we can issue the wake up, we must trigger the next DMA read
        // operation, since the notification might yield the CPU.
        notify_rx_completion = true;
      }
    }
    stream->TriggerReadDma();

    stream->interrupt_lock_.unlock();

    if (notify_rx_completion) {
      rx_data->notification.release();
    }
  } else if (status == kStatus_USART_TxIdle) {
    // Tx transfer
    UsartDmaTxData* tx_data = &stream->tx_data_;
    tx_data->tx_idx += tx_data->transfer.dataSize;
    if (tx_data->tx_idx == tx_data->buffer.size_bytes()) {
      // We have completed the send request, we must wake up the sender.
      tx_data->notification.release();
    } else {
      PW_CHECK_INT_LT(tx_data->tx_idx, tx_data->buffer.size_bytes());
      stream->TriggerWriteDma();
    }
  }
}

// Get the amount of bytes that have been received, but haven't been copied yet
//
// Note: The caller must ensure that the interrupt handler cannot execute.
StatusWithSize DmaUartMcuxpresso::TransferGetReceiveDMACountLockHeld() {
  uint32_t count = 0;

  // If no in-flight transfer is in progress, there is no pending data
  // available. We have initialized count to 0 to account for that.
  (void)USART_TransferGetReceiveCountDMA(
      config_.usart_base, &uart_dma_handle_, &count);

  // We must be executing with the interrupt_lock_ held, so that the interrupt
  // handler cannot change data_received.
  count += rx_data_.data_received - rx_data_.data_copied;
  // Check whether we hit an overflow condition
  if (count > rx_data_.ring_buffer.size_bytes()) {
    return StatusWithSize(Status::DataLoss(), 0);
  }
  return StatusWithSize(count);
}

// Get the amount of bytes that have been received, but haven't been copied yet
StatusWithSize DmaUartMcuxpresso::TransferGetReceiveDMACount() {
  // We need to acquire the interrupt_lock_ , so that the interrupt handler
  // cannot run to change rxRingBufferWriteIdx.
  interrupt_lock_.lock();
  StatusWithSize status = TransferGetReceiveDMACountLockHeld();
  interrupt_lock_.unlock();
  return status;
}

// Get the amount of bytes that have not been yet received for the current
// transfer
//
// Note: This function may only be called once the RX transaction has been
// aborted.
size_t DmaUartMcuxpresso::GetReceiveTransferRemainingBytes() {
  return DMA_GetRemainingBytes(uart_dma_handle_.rxDmaHandle->base,
                               uart_dma_handle_.rxDmaHandle->channel);
}

// Wait for more receive bytes to arrive to satisfy request
//
// Once we have acquired the interrupt_lock_, we check whether we can
// satisfy the request, and if not, we will abort the current
// transaction if the current transaction will be able to satisfy
// the outstanding request. Once the transaction has been aborted
// we can specify the completion_size, so that the completion callback
// can wake us up when the bytes_needed bytes have been received.
//
// If more than one transaction is required to satisfy the request,
// we don't need to abort the transaction and instead can leverage
// the fact that the completion callback won't be triggered since we
// have acquired the interrupt_lock_ . This allows us to specify
// the completion_size that will be seen by the completion callback
// when it executes. A subsequent completion callback will wake us up
// when the bytes_needed have been received.
Status DmaUartMcuxpresso::WaitForReceiveBytes(
    size_t bytes_needed,
    std::optional<chrono::SystemClock::time_point> deadline) {
  // Acquire the interrupt_lock_, so that the interrupt handler cannot
  // execute and modify the shared state.
  interrupt_lock_.lock();

  // Recheck what the current amount of available bytes is.
  StatusWithSize rx_count_status = TransferGetReceiveDMACountLockHeld();
  if (!rx_count_status.ok()) {
    interrupt_lock_.unlock();
    return rx_count_status.status();
  }

  size_t rx_count = rx_count_status.size();
  if (rx_count >= bytes_needed) {
    interrupt_lock_.unlock();
    return OkStatus();
  }

  // Not enough bytes available yet.
  // We check whether more bytes are needed than the transfer's
  // dataSize, which means that at least one more transfer must
  // complete to satisfy this receive request.
  size_t pos_in_transfer =
      rx_data_.data_copied + rx_count - rx_data_.data_received;
  PW_DCHECK_INT_LE(pos_in_transfer, rx_data_.transfer.dataSize);

  size_t transfer_bytes_needed =
      bytes_needed + rx_data_.data_copied - rx_data_.data_received;
  bool aborted = false;

  if (transfer_bytes_needed < rx_data_.transfer.dataSize) {
    // Abort the current transfer, so that we can schedule a receive
    // transfer to satisfy this request.
    USART_TransferAbortReceiveDMA(config_.usart_base, &uart_dma_handle_);
    size_t remaining_transfer_bytes = GetReceiveTransferRemainingBytes();
    if (remaining_transfer_bytes == 0) {
      // We have received all bytes for the current transfer, we will
      // restart the loop in the caller's context.
      // The interrupt handler will execute and call TriggerReadDma
      // to schedule the next receive DMA transfer.
      interrupt_lock_.unlock();
      return OkStatus();
    }
    // We have successfully aborted an in-flight transfer. No interrupt
    // callback will be called for it.
    aborted = true;
    // We need to fix up the transfer size for the aborted transfer.
    rx_data_.transfer.dataSize -= remaining_transfer_bytes;
  } else {
    // We require at least as much data as provided by the current
    // transfer. We know that this code cannot execute while the
    // receive transaction isn't active, so we know that the
    // completion callback will still execute.
  }

  // Tell the transfer callback when to deliver the completion
  // notification.
  rx_data_.completion_size = transfer_bytes_needed;

  // Since a caller could request a receive amount that exceeds the ring
  // buffer size, we must cap the rxCompletionSize. In addition, we
  // don't want that the rxRingBuffer overflows, so we cap the
  // rxCompletionSize to 25% of the ringBufferSize to ensure that the
  // ring buffer gets drained frequently enough.
  if (rx_data_.completion_size >
      rx_data_.ring_buffer.size_bytes() / kUsartRxRingBufferSplitCount) {
    rx_data_.completion_size =
        rx_data_.ring_buffer.size_bytes() / kUsartRxRingBufferSplitCount;
  }

  interrupt_lock_.unlock();

  if (aborted) {
    // We have received data, but we haven't accounted for it, since the
    // callback won't execute due to the abort. Execute the callback
    // from here instead. Since the DMA transfer has been aborted, and
    // the available data isn't sufficient to satisfy this request, the
    // next receive DMA transfer will unblock this thread.
    TxRxCompletionCallback(
        config_.usart_base, &uart_dma_handle_, kStatus_USART_RxIdle, this);
  }

  // Wait for the interrupt handler to deliver the completion
  // notification.
  Status status = OkStatus();
  if (deadline.has_value()) {
    if (!rx_data_.notification.try_acquire_until(*deadline)) {
      // Timeout expired. No need to cancel the DMA; subsequent bytes will
      // just go into the ring buffer.
      status.Update(Status::DeadlineExceeded());
    }
  } else {
    rx_data_.notification.acquire();
  }

  if (!initialized_) {
    // Deinit signaled us.
    status.Update(Status::Cancelled());
  }

  // We have received bytes that can be copied out, we will restart
  // the loop in the caller's context.
  return status;
}

// Copy the data from the receive ring buffer into the destination data buffer
void DmaUartMcuxpresso::CopyReceiveData(ByteBuilder& bb, size_t copy_size) {
  ByteSpan ring_buffer = rx_data_.ring_buffer;
  // Check whether we need to perform a wrap around copy operation or end
  // right at the end of the buffer.
  if (rx_data_.ring_buffer_read_idx + copy_size >=
      rx_data_.ring_buffer.size_bytes()) {
    size_t first_copy_size =
        rx_data_.ring_buffer.size_bytes() - rx_data_.ring_buffer_read_idx;
    bb.append(
        ring_buffer.subspan(rx_data_.ring_buffer_read_idx, first_copy_size));
    size_t second_copy_size = copy_size - first_copy_size;
    // Source buffer is at offset 0.
    bb.append(ring_buffer.subspan(0, second_copy_size));
    rx_data_.ring_buffer_read_idx = second_copy_size;
  } else {
    // Normal copy operation
    PW_DCHECK_INT_LT(rx_data_.ring_buffer_read_idx + copy_size,
                     rx_data_.ring_buffer.size_bytes());
    bb.append(ring_buffer.subspan(rx_data_.ring_buffer_read_idx, copy_size));
    rx_data_.ring_buffer_read_idx += copy_size;
  }
  rx_data_.data_copied += copy_size;
}

Status DmaUartMcuxpresso::DoEnable(bool enable) {
  if (enable == initialized_) {
    return OkStatus();
  }

  // Can't init or deinit if we are active from previous calls.
  if (rx_data_.busy || tx_data_.busy) {
    return Status::FailedPrecondition();
  }

  if (enable) {
    return Init();
  } else {
    Deinit();
    return OkStatus();
  }
}

Status DmaUartMcuxpresso::DoSetBaudRate(uint32_t baud_rate) {
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

Status DmaUartMcuxpresso::DoSetFlowControl(bool enable) {
  config_.flow_control = enable;

  if (!initialized_) {
    return OkStatus();
  }

  USART_EnableCTS(config_.usart_base, enable);
  return OkStatus();
}

// Copy data from the RX ring buffer into the caller provided buffer
//
// If the ring buffer can already satisfy the read request, the
// data will be copied from the ring buffer into the provided buffer.
// If no data, or not sufficient data is available to satisfy the
// read request, the caller will wait for the completion callback to
// signal that data is available and can be copied from the ring buffer
// to the provided buffer.
//
// Note: A reader may request to read more data than can be stored
// inside the RX ring buffer.
//
// Note: Only one thread should be calling this function,
// otherwise DoRead calls might fail due to contention for
// the USART RX channel.
StatusWithSize DmaUartMcuxpresso::DoTryReadFor(
    ByteSpan rx_buffer,
    size_t min_bytes,
    std::optional<chrono::SystemClock::duration> timeout) {
  if (!initialized_) {
    return StatusWithSize(Status::FailedPrecondition(), 0);
  }

  if (timeout.has_value() && *timeout < chrono::SystemClock::duration::zero()) {
    return StatusWithSize(Status::InvalidArgument(), 0);
  }

  size_t length = rx_buffer.size();
  if (length == 0 || min_bytes > length) {
    return StatusWithSize(Status::InvalidArgument(), 0);
  }

  std::optional<chrono::SystemClock::time_point> deadline = std::nullopt;
  if (timeout.has_value()) {
    deadline.emplace(*timeout + chrono::SystemClock::now());
  }

  // We only allow a single thread to read from the USART at a time.
  bool was_busy = rx_data_.busy.exchange(true);
  if (was_busy) {
    return StatusWithSize(Status::FailedPrecondition(), 0);
  }

  Status status = OkStatus();

  ByteBuilder bb(rx_buffer);
  size_t bytes_copied = 0;
  while (bytes_copied < min_bytes) {
    // Get the number of bytes available to read.
    StatusWithSize rx_count_status = TransferGetReceiveDMACount();
    if (!rx_count_status.ok()) {
      status.Update(rx_count_status.status());
      break;
    }
    size_t rx_count = rx_count_status.size();

    // Copy available bytes out of the ring buffer.
    if (rx_count > 0) {
      // Allow copying more than min_bytes if they are available.
      size_t copy_size = std::min(length - bytes_copied, rx_count);
      CopyReceiveData(bb, copy_size);
      bytes_copied += copy_size;
    }

    // Do we still need more bytes?
    // We need to wait for more DMA bytes if so.
    if (bytes_copied < min_bytes) {
      // Check if the timeout has expired, if applicable.
      if (deadline.has_value() && chrono::SystemClock::now() >= *deadline) {
        status.Update(Status::DeadlineExceeded());
        break;
      }

      // Wait up to the timeout duration to receive more bytes.
      Status wait_status =
          WaitForReceiveBytes(min_bytes - bytes_copied, deadline);
      // Even if we exceeded the deadline, stay in the loop for one more
      // iteration to copy out any final bytes.
      if (!wait_status.ok() && !wait_status.IsDeadlineExceeded()) {
        status.Update(wait_status);
        break;
      }

      // At this point, we have new bytes to read, have timed out, or both.
      // Go back to the top of the loop to figure out which.
    }
  }

  rx_data_.busy.store(false);
  return StatusWithSize(status, bytes_copied);
}

// Write data to USART using DMA transactions
//
// Note: Only one thread should be calling this function,
// otherwise DoWrite calls might fail due to contention for
// the USART TX channel.
StatusWithSize DmaUartMcuxpresso::DoTryWriteFor(
    ConstByteSpan tx_buffer,
    std::optional<chrono::SystemClock::duration> timeout) {
  if (!initialized_) {
    return StatusWithSize(Status::FailedPrecondition(), 0);
  }

  if (tx_buffer.size() == 0) {
    return StatusWithSize(0);
  }

  bool was_busy = tx_data_.busy.exchange(true);
  if (was_busy) {
    // Another thread is already transmitting data.
    return StatusWithSize::FailedPrecondition(0);
  }

  // Start the DMA. If multiple DMA transactions are needed, the completion
  // callback will set them up.
  tx_data_.buffer = tx_buffer;
  tx_data_.tx_idx = 0;

  TriggerWriteDma();

  // Wait for completion, optionally with timeout.
  Status status = OkStatus();
  if (timeout.has_value()) {
    if (!tx_data_.notification.try_acquire_for(*timeout)) {
      interrupt_lock_.lock();
      USART_TransferAbortSendDMA(config_.usart_base, &uart_dma_handle_);
      interrupt_lock_.unlock();
      status.Update(Status::DeadlineExceeded());
    }
  } else {
    tx_data_.notification.acquire();
  }

  if (!initialized_) {
    // Deinit signaled us.
    status.Update(Status::Cancelled());
  }

  size_t bytes_written = tx_data_.tx_idx;

  tx_data_.busy.store(false);

  return StatusWithSize(status, bytes_written);
}

size_t DmaUartMcuxpresso::DoConservativeReadAvailable() {
  StatusWithSize status = TransferGetReceiveDMACount();
  if (!status.ok()) {
    return 0;
  }
  return status.size();
}

Status DmaUartMcuxpresso::DoFlushOutput() {
  // Unsupported
  return OkStatus();
}

Status DmaUartMcuxpresso::DoClearPendingReceiveBytes() {
  bool was_busy = rx_data_.busy.exchange(true);
  if (was_busy) {
    return Status::FailedPrecondition();
  }
  size_t bytes_pending = rx_data_.data_received - rx_data_.data_copied;
  if (rx_data_.ring_buffer_read_idx + bytes_pending >=
      rx_data_.ring_buffer.size_bytes()) {
    rx_data_.ring_buffer_read_idx -= rx_data_.ring_buffer.size_bytes();
  }
  rx_data_.ring_buffer_read_idx += bytes_pending;

  rx_data_.data_copied = rx_data_.data_received;
  rx_data_.busy.store(false);

  return OkStatus();
}

}  // namespace pw::uart
