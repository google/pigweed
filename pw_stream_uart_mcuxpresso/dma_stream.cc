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

/*
 * Copyright 2018 - 2022 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pw_stream_uart_mcuxpresso/dma_stream.h"

#include "pw_assert/check.h"
#include "pw_preprocessor/util.h"

namespace pw::stream {

// Deinitialize the DMA channels and USART.
void UartDmaStreamMcuxpresso::Deinit() {
  // We need to touch register space that can be shared
  // among several DMA peripherals, hence we need to access
  // it exclusively. We achieve exclusive access on non-SMP systems as
  // a side effect of acquiring the interrupt_lock_, since acquiring the
  // interrupt_lock_ disables interrupts on the current CPU, which means
  // we cannot get descheduled until we release the interrupt_lock_.
  interrupt_lock_.lock();
  DMA_DisableChannel(config_.dma_base, config_.tx_dma_ch);
  DMA_DisableChannel(config_.dma_base, config_.rx_dma_ch);
  interrupt_lock_.unlock();

  USART_Deinit(config_.usart_base);
}

UartDmaStreamMcuxpresso::~UartDmaStreamMcuxpresso() {
  if (!initialized_) {
    return;
  }
  Deinit();
}

// Initialize the USART and DMA channels based on the configuration
// specified during object creation.
Status UartDmaStreamMcuxpresso::Init(uint32_t srcclk) {
  if (srcclk == 0) {
    return Status::InvalidArgument();
  }
  if (config_.usart_base == nullptr) {
    return Status::InvalidArgument();
  }
  if (config_.baud_rate == 0) {
    return Status::InvalidArgument();
  }
  if (config_.dma_base == nullptr) {
    return Status::InvalidArgument();
  }

  usart_config_t defconfig_;
  USART_GetDefaultConfig(&defconfig_);

  defconfig_.baudRate_Bps = config_.baud_rate;
  defconfig_.parityMode = config_.parity;
  defconfig_.enableTx = true;
  defconfig_.enableRx = true;

  status_t status = USART_Init(config_.usart_base, &defconfig_, srcclk);
  if (status != kStatus_Success) {
    return Status::Internal();
  }

  // We need to touch register space that can be shared
  // among several DMA peripherals, hence we need to access
  // it exclusively. We achieve exclusive access on non-SMP systems as
  // a side effect of acquiring the interrupt_lock_, since acquiring the
  // interrupt_lock_ disables interrupts on the current CPU, which means
  // we cannot get descheduled until we release the interrupt_lock_.
  interrupt_lock_.lock();

  INPUTMUX_Init(INPUTMUX);
  // Enable DMA request.
  INPUTMUX_EnableSignal(
      INPUTMUX, config_.rx_input_mux_dmac_ch_request_en, true);
  INPUTMUX_EnableSignal(
      INPUTMUX, config_.tx_input_mux_dmac_ch_request_en, true);
  // Turnoff clock to inputmux to save power. Clock is only needed to make
  // changes.
  INPUTMUX_Deinit(INPUTMUX);

  DMA_EnableChannel(config_.dma_base, config_.tx_dma_ch);
  DMA_EnableChannel(config_.dma_base, config_.rx_dma_ch);

  DMA_CreateHandle(&tx_data_.dma_handle, config_.dma_base, config_.tx_dma_ch);
  DMA_CreateHandle(&rx_data_.dma_handle, config_.dma_base, config_.rx_dma_ch);

  interrupt_lock_.unlock();

  status = USART_TransferCreateHandleDMA(config_.usart_base,
                                         &uart_dma_handle_,
                                         TxRxCompletionCallback,
                                         this,
                                         &tx_data_.dma_handle,
                                         &rx_data_.dma_handle);

  if (status != kStatus_Success) {
    Deinit();
    return Status::Internal();
  }

  // Read into the rx ring buffer.
  interrupt_lock_.lock();
  TriggerReadDma();
  interrupt_lock_.unlock();

  initialized_ = true;
  return OkStatus();
}

// DMA usart data into ring buffer
//
// At most kUsartDmaMaxTransferCount bytes can be copied per DMA transfer.
// If completion_size is specified and dataSize is larger than completion_size,
// the dataSize will be limited to completion_size so that the completion
// callback will be called once completion_size bytes have been received.
void UartDmaStreamMcuxpresso::TriggerReadDma() {
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
void UartDmaStreamMcuxpresso::TriggerWriteDma() {
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
void UartDmaStreamMcuxpresso::TxRxCompletionCallback(USART_Type* base,
                                                     usart_dma_handle_t* state,
                                                     status_t status,
                                                     void* param) {
  UartDmaStreamMcuxpresso* stream =
      reinterpret_cast<UartDmaStreamMcuxpresso*>(param);

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
        // We have satisified the receive request, we must wake up the receiver.
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
StatusWithSize UartDmaStreamMcuxpresso::TransferGetReceiveDMACountLockHeld() {
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
StatusWithSize UartDmaStreamMcuxpresso::TransferGetReceiveDMACount() {
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
size_t UartDmaStreamMcuxpresso::GetReceiveTransferRemainingBytes() {
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
Status UartDmaStreamMcuxpresso::WaitForReceiveBytes(size_t bytes_needed) {
  // Acquire the interrupt_lock_, so that the interrupt handler cannot
  // execute and modify the shared state.
  interrupt_lock_.lock();

  // Recheck what the current amount of available bytes is.
  StatusWithSize status = TransferGetReceiveDMACountLockHeld();
  if (!status.ok()) {
    interrupt_lock_.unlock();
    return status.status();
  }

  size_t rx_count = status.size();
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
  // notificiation.
  rx_data_.notification.acquire();
  // We have received bytes that can be copied out, we will restart
  // the loop in the caller's context.
  return OkStatus();
}

// Copy the data from the receive ring buffer into the destination data buffer
void UartDmaStreamMcuxpresso::CopyReceiveData(ByteBuilder& bb,
                                              size_t copy_size) {
  ByteSpan ring_buffer = rx_data_.ring_buffer;
  reinterpret_cast<uint8_t*>(rx_data_.ring_buffer.data());
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
StatusWithSize UartDmaStreamMcuxpresso::DoRead(ByteSpan data) {
  size_t length = data.size();
  if (length == 0) {
    return StatusWithSize(Status::InvalidArgument(), 0);
  }

  // We only allow a single thread to read from the USART at a time.
  bool was_busy = rx_data_.busy.exchange(true);
  if (was_busy) {
    return StatusWithSize(Status::FailedPrecondition(), 0);
  }

  size_t rx_count = 0;
  ByteBuilder bb(data);

  for (size_t buf_idx = 0; buf_idx < length;) {
    size_t bytes_needed = length - buf_idx;

    while (rx_count == 0) {
      StatusWithSize status_with_size = TransferGetReceiveDMACount();
      if (!status_with_size.ok()) {
        rx_data_.busy.store(false);
        return StatusWithSize(status_with_size.status(), buf_idx);
      }
      rx_count = status_with_size.size();
      if (rx_count < bytes_needed) {
        // Wait to receive more bytes.
        Status status = WaitForReceiveBytes(bytes_needed);
        if (!status.ok()) {
          rx_data_.busy.store(false);
          return StatusWithSize(status, buf_idx);
        }
        // Restart the loop and refetch rx_count. We should be able
        // to copy out data to the destination data buffer.
        rx_count = 0;
        continue;
      }
    }

    size_t copy_size = MIN(bytes_needed, rx_count);
    CopyReceiveData(bb, copy_size);
    buf_idx += copy_size;
    PW_DCHECK(rx_count == copy_size || buf_idx == length);
  }

  rx_data_.busy.store(false);
  return StatusWithSize(length);
}

// Write data to USART using DMA transactions
//
// Note: Only one thread should be calling this function,
// otherwise DoWrite calls might fail due to contention for
// the USART TX channel.
Status UartDmaStreamMcuxpresso::DoWrite(ConstByteSpan data) {
  if (data.size() == 0) {
    return Status::InvalidArgument();
  }

  bool was_busy = tx_data_.busy.exchange(true);
  if (was_busy) {
    // Another thread is already transmitting data.
    return Status::FailedPrecondition();
  }

  tx_data_.buffer = data;
  tx_data_.tx_idx = 0;

  TriggerWriteDma();

  tx_data_.notification.acquire();

  tx_data_.busy.store(false);

  return OkStatus();
}

}  // namespace pw::stream
