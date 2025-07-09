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

#include "pw_uart_mcuxpresso/uart_nonblocking.h"

#include "fsl_flexcomm.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"

namespace pw::uart {

// Deinitialize the USART.
void UartMcuxpressoNonBlocking::Deinit() {
  std::lock_guard lock(interrupt_lock_);

  if (!initialized_) {
    return;
  }

  DoCancelWriteLockHeld();
  DoCancelFlushOutputLockHeld();

  DoCancelReadLockHeld();
  // Cancel read into ring buffer as DoCancelRead starts it again.
  USART_TransferAbortReceive(config_.usart_base, &uart_handle_);

  USART_Deinit(config_.usart_base);
  clock_tree_element_controller_.Release().IgnoreError();
  initialized_ = false;
}

UartMcuxpressoNonBlocking::~UartMcuxpressoNonBlocking() { Deinit(); }

// Initialize the USART based on the configuration
// specified during object creation.
Status UartMcuxpressoNonBlocking::Init() {
  {
    std::lock_guard lock(interrupt_lock_);
    if (initialized_) {
      return Status::FailedPrecondition();
    }
  }

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

  {
    std::lock_guard lock(interrupt_lock_);
    // Initialized enough for Deinit code to handle any errors from here.
    initialized_ = true;
  }

  status = USART_TransferCreateHandle(
      config_.usart_base, &uart_handle_, UsartCallback, this);

  if (status != kStatus_Success) {
    Deinit();
    return Status::Internal();
  }

  {
    std::lock_guard lock(interrupt_lock_);

    rx_data_.request.valid = false;
    tx_data_.request.valid = false;
    tx_data_.flush_request.valid = false;

    // Begin reading into the ring buffer.
    USART_TransferStartRingBuffer(
        config_.usart_base,
        &uart_handle_,
        reinterpret_cast<uint8_t*>(config_.buffer.data()),
        config_.buffer.size());
  }

  return OkStatus();
}

Status UartMcuxpressoNonBlocking::DoEnable(bool enable) {
  {
    std::lock_guard lock(interrupt_lock_);
    if (enable == initialized_) {
      return OkStatus();
    }
  }

  if (enable) {
    return Init();
  } else {
    Deinit();
    return OkStatus();
  }
}

// Trigger a TX DMA from the user's buffer.
void UartMcuxpressoNonBlocking::TriggerWrite() {
  PW_DCHECK(tx_data_.request.valid);
}

Status UartMcuxpressoNonBlocking::DoRead(
    ByteSpan rx_buffer,
    size_t min_bytes,
    Function<void(Status status, ConstByteSpan buffer)>&& callback) {
  size_t max_bytes = rx_buffer.size();
  if (min_bytes == 0 || max_bytes == 0 || min_bytes > max_bytes) {
    return Status::InvalidArgument();
  }

  // We must grab the interrupt lock before reading the `valid` flag to avoid
  // racing with `TxRxCompletionCallback()`.
  {
    std::lock_guard lock(interrupt_lock_);

    if (!initialized_) {
      return Status::FailedPrecondition();
    }

    if (rx_data_.request.valid) {
      return Status::Unavailable();
    }

    PW_LOG_DEBUG("DoRead: size(%u)", min_bytes);

    rx_data_.request.valid = true;
    rx_data_.request.buffer = rx_buffer;
    rx_data_.request.size = min_bytes;
    rx_data_.request.callback = std::move(callback);

    rx_data_.transfer.data =
        reinterpret_cast<uint8_t*>(rx_data_.request.buffer.data());
    rx_data_.transfer.dataSize = rx_data_.request.size;
  }

  // Call outside of interrupt lock since it could call our callback right away.
  //
  // This should only fail if we try and start a transfer when already started,
  // which would be a bug in this driver.
  PW_CHECK(USART_TransferReceiveNonBlocking(config_.usart_base,
                                            &uart_handle_,
                                            &rx_data_.transfer,
                                            nullptr) == kStatus_Success);
  return OkStatus();
}

Status UartMcuxpressoNonBlocking::DoWrite(
    ConstByteSpan tx_buffer, Function<void(StatusWithSize status)>&& callback) {
  if (tx_buffer.size() == 0) {
    return Status::InvalidArgument();
  }

  PW_LOG_DEBUG("DoWrite: size(%u)", tx_buffer.size());

  std::lock_guard lock(interrupt_lock_);

  if (!initialized_) {
    return Status::FailedPrecondition();
  }

  if (tx_data_.request.valid) {
    return Status::Unavailable();
  }
  tx_data_.request.valid = true;
  tx_data_.request.buffer = tx_buffer;
  tx_data_.request.callback = std::move(callback);

  tx_data_.transfer.txData =
      reinterpret_cast<const uint8_t*>(tx_data_.request.buffer.data());
  tx_data_.transfer.dataSize = tx_data_.request.buffer.size();

  // This should only fail if we try and start a transfer when already started,
  // which would be a bug in this driver.
  PW_CHECK(USART_TransferSendNonBlocking(
               config_.usart_base, &uart_handle_, &tx_data_.transfer) ==
           kStatus_Success);
  return OkStatus();
}

// Static wrapper method called by the completion ISR.
void UartMcuxpressoNonBlocking::UsartCallback(USART_Type* base,
                                              usart_handle_t* handle,
                                              status_t status,
                                              void* userdata) {
  auto* uart = static_cast<UartMcuxpressoNonBlocking*>(userdata);
  PW_CHECK_PTR_EQ(base, uart->config_.usart_base);
  PW_CHECK_PTR_EQ(handle, &uart->uart_handle_);

  return uart->TxRxCompletionCallback(status);
}

// This callback is called by both the RX and TX interrupt handlers.
// It is called upon completion of a transfer.
void UartMcuxpressoNonBlocking::TxRxCompletionCallback(status_t status) {
  std::lock_guard lock(interrupt_lock_);

  if (status == kStatus_USART_RxIdle && rx_data_.request.valid) {
    // RX transaction complete
    // Call the user's completion callback and invalidate the request.
    rx_data_.request.callback(
        OkStatus(), rx_data_.request.buffer.first(rx_data_.request.size));
    rx_data_.request.valid = false;
  }

  if (status == kStatus_USART_TxIdle && tx_data_.request.valid) {
    // TX transaction complete
    tx_data_.request.callback(StatusWithSize(tx_data_.request.buffer.size()));
    tx_data_.request.valid = false;
    CompleteFlushRequest(OkStatus());
  }
}

bool UartMcuxpressoNonBlocking::DoCancelRead() {
  std::lock_guard lock(interrupt_lock_);
  return DoCancelReadLockHeld();
}

bool UartMcuxpressoNonBlocking::DoCancelReadLockHeld() {
  if (!rx_data_.request.valid) {
    return false;
  }

  // Cancel the in-flight transfer.
  USART_TransferAbortReceive(config_.usart_base, &uart_handle_);

  rx_data_.request.callback(Status::Cancelled(), {});
  rx_data_.request.valid = false;

  // Ring buffer still running

  return true;
}

bool UartMcuxpressoNonBlocking::DoCancelWrite() {
  std::lock_guard lock(interrupt_lock_);
  return DoCancelWriteLockHeld();
}

bool UartMcuxpressoNonBlocking::DoCancelWriteLockHeld() {
  if (!tx_data_.request.valid) {
    return false;
  }

  // There is a TX in-flight.
  // We know this because we are in a critical section and the request is valid.

  // Cancel the in-flight transfer.
  USART_TransferAbortSend(config_.usart_base, &uart_handle_);

  tx_data_.request.callback(StatusWithSize::Cancelled(0));
  tx_data_.request.valid = false;

  CompleteFlushRequest(Status::Aborted());

  return true;
}

size_t UartMcuxpressoNonBlocking::DoConservativeReadAvailable() {
  return USART_TransferGetRxRingBufferLength(&uart_handle_);
}

Status UartMcuxpressoNonBlocking::DoClearPendingReceiveBytes() {
  std::lock_guard lock(interrupt_lock_);
  return DoClearPendingReceiveBytesLockHeld();
}

Status UartMcuxpressoNonBlocking::DoClearPendingReceiveBytesLockHeld() {
  if (!initialized_) {
    return OkStatus();
  }

  if (rx_data_.request.valid) {
    return Status::FailedPrecondition();
  }

  USART_TransferStopRingBuffer(config_.usart_base, &uart_handle_);
  USART_TransferStartRingBuffer(
      config_.usart_base,
      &uart_handle_,
      reinterpret_cast<uint8_t*>(config_.buffer.data()),
      config_.buffer.size());

  return OkStatus();
}

Status UartMcuxpressoNonBlocking::DoSetBaudRate(uint32_t baud_rate) {
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

Status UartMcuxpressoNonBlocking::DoSetFlowControl(bool enable) {
  config_.flow_control = enable;

  if (initialized_) {
    USART_EnableCTS(config_.usart_base, enable);
  }

  return OkStatus();
}

bool UartMcuxpressoNonBlocking::CompleteFlushRequest(Status status) {
  if (!tx_data_.flush_request.valid) {
    return false;
  }

  tx_data_.flush_request.callback(status);
  tx_data_.flush_request.valid = false;
  tx_data_.flush_request.callback = nullptr;

  PW_DCHECK(USART_FIFOSTAT_TXEMPTY(config_.usart_base->FIFOSTAT));

  return true;
}

Status UartMcuxpressoNonBlocking::DoFlushOutput(
    Function<void(Status status)>&& callback) {
  std::lock_guard lock(interrupt_lock_);

  if (tx_data_.flush_request.valid) {
    return Status::FailedPrecondition();
  }

  if (!tx_data_.request.valid) {
    callback(OkStatus());
    return OkStatus();
  }

  tx_data_.flush_request.callback = std::move(callback);
  tx_data_.flush_request.valid = true;

  return OkStatus();
}

bool UartMcuxpressoNonBlocking::DoCancelFlushOutput() {
  std::lock_guard lock(interrupt_lock_);
  return DoCancelFlushOutputLockHeld();
}

bool UartMcuxpressoNonBlocking::DoCancelFlushOutputLockHeld() {
  return CompleteFlushRequest(Status::Cancelled());
}

}  // namespace pw::uart
