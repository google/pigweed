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
#include "pw_spi_mcuxpresso/responder.h"

#include <cinttypes>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

// Vendor terminology requires this to be disabled.
// inclusive-language: disable

namespace pw::spi {
namespace {

uint8_t* SpanData(ByteSpan& span) {
  static_assert(std::is_same_v<uint8_t, unsigned char>);
  return reinterpret_cast<uint8_t*>(span.data());
}

uint8_t* SpanDataDiscardConst(ConstByteSpan& span) {
  static_assert(std::is_same_v<uint8_t, unsigned char>);
  return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(span.data()));
}

Status ToPwStatus(status_t status) {
  switch (status) {
    // Intentional fall-through
    case kStatus_Success:
    case kStatus_SPI_Idle:
      return OkStatus();
    case kStatus_ReadOnly:
      return Status::PermissionDenied();
    case kStatus_OutOfRange:
      return Status::OutOfRange();
    case kStatus_InvalidArgument:
      return Status::InvalidArgument();
    case kStatus_Timeout:
      return Status::DeadlineExceeded();
    case kStatus_NoTransferInProgress:
      return Status::FailedPrecondition();
    // Intentional fall-through
    case kStatus_Fail:
    default:
      PW_LOG_ERROR("Mcuxpresso SPI unknown error code: %d",
                   static_cast<int>(status));
      return Status::Unknown();
  }
}

Status SetSdkConfig(const McuxpressoResponder::Config& config,
                    spi_slave_config_t& sdk_config) {
  switch (config.polarity) {
    case ClockPolarity::kActiveLow:
      sdk_config.polarity = kSPI_ClockPolarityActiveLow;
      break;
    case ClockPolarity::kActiveHigh:
      sdk_config.polarity = kSPI_ClockPolarityActiveHigh;
      break;
    default:
      return Status::InvalidArgument();
  }

  switch (config.phase) {
    case ClockPhase::kRisingEdge:
      sdk_config.phase = kSPI_ClockPhaseFirstEdge;
      break;
    case ClockPhase::kFallingEdge:
      sdk_config.phase = kSPI_ClockPhaseSecondEdge;
      break;
    default:
      return Status::InvalidArgument();
  }

  switch (config.bit_order) {
    case BitOrder::kMsbFirst:
      sdk_config.direction = kSPI_MsbFirst;
      break;
    case BitOrder::kLsbFirst:
      sdk_config.direction = kSPI_LsbFirst;
      break;
    default:
      return Status::InvalidArgument();
  }

  switch (config.bits_per_word()) {
    case 4:
      sdk_config.dataWidth = kSPI_Data4Bits;
      break;
    case 5:
      sdk_config.dataWidth = kSPI_Data5Bits;
      break;
    case 6:
      sdk_config.dataWidth = kSPI_Data6Bits;
      break;
    case 7:
      sdk_config.dataWidth = kSPI_Data7Bits;
      break;
    case 8:
      sdk_config.dataWidth = kSPI_Data8Bits;
      break;
    case 9:
      sdk_config.dataWidth = kSPI_Data9Bits;
      break;
    case 10:
      sdk_config.dataWidth = kSPI_Data10Bits;
      break;
    case 11:
      sdk_config.dataWidth = kSPI_Data11Bits;
      break;
    case 12:
      sdk_config.dataWidth = kSPI_Data12Bits;
      break;
    case 13:
      sdk_config.dataWidth = kSPI_Data13Bits;
      break;
    case 14:
      sdk_config.dataWidth = kSPI_Data14Bits;
      break;
    case 15:
      sdk_config.dataWidth = kSPI_Data15Bits;
      break;
    case 16:
      sdk_config.dataWidth = kSPI_Data16Bits;
      break;
    default:
      return Status::InvalidArgument();
  }

  return OkStatus();
}

//
// Helpful things missing from the SDK
//
const IRQn_Type spi_irq_map[] = SPI_IRQS;

// Enable interrupt on CS asserted / de-asserted.
void SPI_EnableSSInterrupt(SPI_Type* base) {
  base->STAT = SPI_STAT_SSA_MASK | SPI_STAT_SSD_MASK;  // Clear first
  base->INTENSET = SPI_INTENSET_SSAEN_MASK | SPI_INTENSET_SSDEN_MASK;
}

// Disable interrupt on CS asserted / de-asserted.
void SPI_DisableSSInterrupt(SPI_Type* base) {
  base->INTENCLR = SPI_INTENSET_SSAEN_MASK | SPI_INTENSET_SSDEN_MASK;
}

// Empty the TX and RX FIFOs.
void SPI_EmptyFifos(SPI_Type* base) {
  base->FIFOCFG |= SPI_FIFOCFG_EMPTYTX_MASK | SPI_FIFOCFG_EMPTYRX_MASK;
}

bool SPI_RxFifoIsEmpty(SPI_Type* base) {
  // RXNOTEMPTY: Receive FIFO is Not Empty
  // 0 - The receive FIFO is empty.
  // 1 - The receive FIFO is not empty, so data can be read.
  return !(base->FIFOSTAT & SPI_FIFOSTAT_RXNOTEMPTY_MASK);
}

// Non-FIFO interrupt sources
enum _spi_interrupt_sources {
  kSPI_SlaveSelAssertIrq = SPI_INTENSET_SSAEN_MASK,
  kSPI_SlaveSelDeassertIrq = SPI_INTENSET_SSDEN_MASK,
  kSPI_MasterIdleIrq = SPI_INTENSET_MSTIDLEEN_MASK,
};

// Gets a bitmap of active (pending + enabled) interrupts.
// Test against _spi_interrupt_sources constants.
uint32_t SPI_GetActiveInterrupts(SPI_Type* base) {
  // Verify that the bits in INTSTAT and INTENSET are the same.
  static_assert(SPI_INTSTAT_SSA_MASK == SPI_INTENSET_SSAEN_MASK);
  static_assert(SPI_INTSTAT_SSD_MASK == SPI_INTENSET_SSDEN_MASK);
  static_assert(SPI_INTSTAT_MSTIDLE_MASK == SPI_INTENSET_MSTIDLEEN_MASK);
  return base->INTSTAT & base->INTENSET;
}

// Clears a bitmap of active interrupts.
// This acknowledges the interrupt; it does not disable it.
// @irqs is either kSPI_SlaveSelAssertIrq or kSPI_SlaveSelDeassertIrq.
void SPI_ClearActiveInterrupts(SPI_Type* base, uint32_t irqs) {
  // Verify that the bits in STAT match the enum.
  static_assert(SPI_STAT_SSA_MASK == kSPI_SlaveSelAssertIrq);
  static_assert(SPI_STAT_SSD_MASK == kSPI_SlaveSelDeassertIrq);
  PW_CHECK((irqs & ~(kSPI_SlaveSelAssertIrq | kSPI_SlaveSelDeassertIrq)) == 0);
  base->STAT = irqs;  // write to clear
}

}  // namespace

Status McuxpressoResponder::Initialize() {
  status_t sdk_status;
  spi_slave_config_t sdk_config;
  spi_dma_callback_t callback;

  SPI_SlaveGetDefaultConfig(&sdk_config);
  PW_TRY(SetSdkConfig(config_, sdk_config));

  // Hard coded for now, till added to Config
  sdk_config.sselPol = kSPI_SpolActiveAllLow;

  sdk_status = SPI_SlaveInit(base_, &sdk_config);
  if (sdk_status != kStatus_Success) {
    PW_LOG_ERROR("SPI_SlaveInit failed: %ld", sdk_status);
    return ToPwStatus(sdk_status);
  }

  if (config_.handle_cs) {
    // Set up the FLEXCOMM IRQ to get CS assertion/deassertion.
    // See SPI_MasterTransferCreateHandle().
    // Note that the 'handle' argument can actually be anything.
    FLEXCOMM_SetIRQHandler(base_, FlexcommSpiIrqHandler, this);

    // Enable SPI interrupt in NVIC
    uint32_t instance = SPI_GetInstance(base_);
    EnableIRQ(spi_irq_map[instance]);

    // We only use the CS deassertion interrupt to complete transfers.
    // Don't provide any callback to the SPI driver (to be invoked by DMA IRQ).
    callback = nullptr;

    // Disable the DMA channel interrupts.
    // If we leave them enabled, then the SPI driver could complete a full
    // transfer, move the state to kSPI_Idle, and prevent
    // SPI_SlaveTransferGetCountDMA() from working.
    rx_dma_.DisableInterrupts();
    tx_dma_.DisableInterrupts();
  } else {
    // Without CS deassertion, we use the SPI driver callback (invoked by DMA
    // IRQ) to complete transfers.
    callback = McuxpressoResponder::SdkCallback;

    // Enable the DMA channel interrupts.
    // These are enabled by default by DMA_CreateHandle(), but re-enable them
    // anyway in case they were disabled for some reason.
    rx_dma_.EnableInterrupts();
    tx_dma_.EnableInterrupts();
  }

  sdk_status = SPI_SlaveTransferCreateHandleDMA(
      base_, &handle_, callback, this, tx_dma_.handle(), rx_dma_.handle());
  if (sdk_status != kStatus_Success) {
    PW_LOG_ERROR("SPI_SlaveTransferCreateHandleDMA failed: %ld", sdk_status);
    return ToPwStatus(sdk_status);
  }

  return OkStatus();
}

void McuxpressoResponder::TransferComplete(Status status,
                                           size_t bytes_transferred) {
  if (config_.handle_cs) {
    SPI_DisableSSInterrupt(base_);
  }

  // Abort the DMA transfer (if active).
  SPI_SlaveTransferAbortDMA(base_, &handle_);

  // Check for TX underflow / RX overflow
  // TODO(jrreinhart): Unfortunately we can't do this. We want to check for
  // FIFO under/overflow *while* the transfer is running, but if the initiator
  // sent more bytes than the DMA was set up to receive, both of these errors
  // will happen (after the DMA is complete). We would need to find a way to
  // capture this status immediate when the DMA is complete, or otherwise
  // monitor it during the transfer.
#if 0
  if (status.ok()) {
    if (SPI_RxError(base_)) {
      PW_LOG_ERROR("RX FIFO overflow detected!");
      status = Status::DataLoss();
    }
    if (SPI_TxError(base_)) {
      PW_LOG_ERROR("TX FIFO underflow detected!");
      status = Status::DataLoss();
    }
  }
#endif

  // TODO(jrreinhart) Remove these safety checks.
  if (rx_dma_.IsBusy()) {
    PW_LOG_WARN("After completion, rx_dma still busy!");
  }
  if (rx_dma_.IsActive()) {
    PW_LOG_WARN("After completion, rx_dma still active!");
  }

  // Empty the FIFOs.
  // If the initiator sent more bytes than the DMA was set up to receive, the
  // RXFIFO will have the residue. This isn't strictly necessary since they'll
  // be cleared on the next call to SPI_SlaveTransferDMA(), but we do it anyway
  // for cleanliness.
  SPI_EmptyFifos(base_);

  // Clear the FIFO DMA request signals.
  //
  // From IMXRT500RM 53.4.2.1.2 DMA operation:
  // "A DMA request is provided for each SPI direction, and can be used instead
  // of interrupts for transferring data... The DMA controller provides an
  // acknowledgement signal that clears the related request when it (the DMA
  // controller) completes handling that request."
  //
  // If the initiator sent more bytes than the DMA was set up to receive, this
  // request signal will remain latched on, even after the FIFO is emptied.
  // This would cause a subsequent transfer to receive one stale residual byte
  // from this prior transfer.
  //
  // We force if off here by disabling the DMA request signal.
  // It will be re-enabled on the next transfer.
  SPI_EnableRxDMA(base_, false);
  SPI_EnableTxDMA(base_, false);

  // Invoke the callback
  auto received = current_transaction_.rx_data.subspan(0, bytes_transferred);
  current_transaction_ = {};
  completion_callback_(received, status);
}

void McuxpressoResponder::SdkCallback(SPI_Type* base,
                                      spi_dma_handle_t* handle,
                                      status_t sdk_status,
                                      void* userData) {
  // WARNING: This is called in IRQ context.
  auto* responder = static_cast<McuxpressoResponder*>(userData);
  PW_CHECK_PTR_EQ(base, responder->base_);
  PW_CHECK_PTR_EQ(handle, &responder->handle_);

  return responder->DmaComplete(sdk_status);
}

void McuxpressoResponder::DmaComplete(status_t sdk_status) {
  // WARNING: This is called in IRQ context.
  PW_LOG_DEBUG("DmaComplete in state=%s with sdk_status=%" PRId32,
               state_str(),
               sdk_status);

  PW_CHECK(!config_.handle_cs,
           "DmaComplete should never be called when handle_cs=true!");

  // Move to idle state.
  if (State prev; !TryChangeState(State::kBusy, State::kIdle, &prev)) {
    // Spurious callback? Or race condition in DoWriteReadAsync()?
    PW_LOG_WARN("DmaComplete not in busy state, but %u",
                static_cast<unsigned int>(prev));
    return;
  }

  // Transfer complete.
  auto status = ToPwStatus(sdk_status);
  size_t bytes_transferred =
      status.ok() ? current_transaction_.rx_data.size() : 0;
  PW_LOG_DEBUG("DmaComplete calling TransferComplete");
  TransferComplete(status, bytes_transferred);
}

void McuxpressoResponder::FlexcommSpiIrqHandler(void* base, void* arg) {
  // WARNING: This is called in IRQ context.

  SPI_Type* spi = static_cast<SPI_Type*>(base);
  auto* responder = static_cast<McuxpressoResponder*>(arg);
  PW_CHECK_PTR_EQ(spi, responder->base_);

  // NOTE: It's possible that CS could deassert and INTSTAT.SSD could latch
  // shortly after the IRQ handler is entered (due to INTSTAT.SSA), re-setting
  // the IRQ as pending in the NVIC. In this case, we could handle both SSA and
  // SSD in the same interrupt. When that happens, the IRQ remains pended in
  // the NVIC, and the handler will file again. We simply ignore the second
  // interrupt.
  //
  // It would wrong to try and handle only one of SSA or SSD per invocation
  // because if the interrupt was handled late enough, it might only fire once.
  const auto active_irqs = SPI_GetActiveInterrupts(spi);

  // CS asserted?
  if (active_irqs & kSPI_SlaveSelAssertIrq) {
    SPI_ClearActiveInterrupts(spi, kSPI_SlaveSelAssertIrq);
    responder->CsAsserted();
  }

  // CS de-asserted?
  if (active_irqs & kSPI_SlaveSelDeassertIrq) {
    SPI_ClearActiveInterrupts(spi, kSPI_SlaveSelDeassertIrq);
    responder->CsDeasserted();
  }
}

void McuxpressoResponder::CsAsserted() {
  // WARNING: This is called in IRQ context.
  PW_LOG_DEBUG("CS asserted! state=%s", state_str());
}

Status McuxpressoResponder::WaitForQuiescenceAfterCsDeassertion() {
  // When CS is deasserted, the master is indicating that it has finished
  // clocking out data into our FIFO. That could be more, less, or the same
  // number of bytes requested by the user (in DoWriteReadAsync).
  //
  // Definitions:
  //   S: The DMA transfer size (as requested by the user).
  //   M: The number of bytes sent by the master.
  //
  // Case | Condition | DMA will complete? | FIFO will empty?
  // -----|-----------|--------------------|-------------------
  //    1 |   M < S   | No                 | Yes
  //    2 |   M = S   | Yes                | Yes
  //    3 |   M > S   | Yes                | No
  //
  // At this point, the RX FIFO might still have data that the DMA has not yet
  // read.
  //
  // We wait for either the DMA channel to become inactive (case 2 or 3) or for
  // the RX FIFO to become empty (case 1 or 2). When the FIFO empties, we also
  // need to wait for the DMA channel to be non-busy, indicating that it has
  // finished moving the data to SRAM.
  //
  // It is expected that by the time this function is called, the hardware will
  // have already quiesced, and we won't actually wait at all. A warning log
  // will indicate if that assumption does not hold true.
  constexpr unsigned int kMaxWaitCount = 10000;  // Arbitrary

  unsigned int wait_count;
  for (wait_count = 0; wait_count < kMaxWaitCount; ++wait_count) {
    if (!rx_dma_.IsActive()) {
      // The DMA has consumed as many bytes from the FIFO as it ever will.
      PW_LOG_DEBUG("CsDeasserted: DMA done");
      break;
    }

    if (SPI_RxFifoIsEmpty(base_) && !rx_dma_.IsBusy()) {
      // The FIFO is empty, and the DMA channel has moved all data to SRAM.
      PW_LOG_DEBUG("CsDeasserted: FIFO empty and DMA not busy");
      break;
    }

    // DMA is still active and FIFO is not empty. We need to wait.
  }

  if (wait_count == kMaxWaitCount) {
    PW_LOG_ERROR(
        "After CS de-assertion, timed out waiting for DMA done or FIFO empty.");
    return Status::DeadlineExceeded();
  }

  if (wait_count != 0) {
    PW_LOG_WARN(
        "After CS de-assertion, waited %u times for DMA done or FIFO empty.",
        wait_count);
  }
  return OkStatus();
}

void McuxpressoResponder::CsDeasserted() {
  // WARNING: This is called in IRQ context.
  PW_LOG_DEBUG("CS deasserted! state=%s", state_str());

  PW_CHECK(config_.handle_cs,
           "CsDeasserted should only be called when handle_cs=true!");

  // Move to idle state.
  if (State prev; !TryChangeState(State::kBusy, State::kIdle, &prev)) {
    PW_LOG_WARN("CsDeasserted not in busy state, but %u",
                static_cast<unsigned int>(prev));
    return;
  }

  Status wait_status = WaitForQuiescenceAfterCsDeassertion();

  // Get the number of bytes actually transferred.
  //
  // NOTE: SPI_SlaveTransferGetCountDMA() fails if _handle.state != kSPI_Busy.
  // Thus, it must be called before SPI_SlaveTransferAbortDMA() which changes
  // the state to kSPI_Idle. Also, the DMA channel interrupts are disabled when
  // CS is respected, because SPI_RxDMACallback() and SPI_TxDMACallback() also
  // change the state to kSPI_Idle.
  size_t bytes_transferred = 0;
  status_t sdk_status =
      SPI_SlaveTransferGetCountDMA(base_, &handle_, &bytes_transferred);

  // Transfer complete.
  Status xfer_status = OkStatus();
  if (!wait_status.ok()) {
    bytes_transferred = 0;
    xfer_status = wait_status;
  } else if (sdk_status != kStatus_Success) {
    PW_LOG_ERROR("SPI_SlaveTransferGetCountDMA() returned %" PRId32,
                 sdk_status);
    bytes_transferred = 0;
    xfer_status = ToPwStatus(sdk_status);
  }
  PW_LOG_DEBUG(
      "CsDeasserted calling TransferComplete(status=%s, bytes_transferred=%zd)"
      " in state=%s",
      xfer_status.str(),
      bytes_transferred,
      state_str());
  TransferComplete(xfer_status, bytes_transferred);
}

Status McuxpressoResponder::DoWriteReadAsync(ConstByteSpan tx_data,
                                             ByteSpan rx_data) {
  if (!TryChangeState(State::kIdle, State::kBusy)) {
    PW_LOG_ERROR("Transaction already started");
    return Status::FailedPrecondition();
  }
  PW_CHECK(!current_transaction_);

  // TODO(jrreinhart): There is a race here. If DoCancel() is called, it will
  // move to kIdle, and invoke the callback with CANCELLED. But then we will
  // still go on to perform the transfer anyway. When the transfer completes,
  // SdkCallback will see kIdle and skip the callback. We avoid this problem
  // by saying that DoWriteReadAsync() and DoCancel() should not be called from
  // different threads, thus we only have to worry about DoCancel() racing the
  // hardware / IRQ.

  spi_transfer_t transfer = {};

  if (!tx_data.empty() && !rx_data.empty()) {
    // spi_transfer_t has only a single dataSize member, so tx_data and
    // rx_data must be the same size. Separate rx/tx data sizes could
    // theoretically be handled, but the SDK doesn't support it.
    //
    // TODO(jrreinhart) Support separate rx/tx data sizes.
    // For non-DMA, it's a pretty simple patch.
    // It should be doable for DMA also, but I haven't looked into it.
    if (tx_data.size() != rx_data.size()) {
      return Status::InvalidArgument();
    }

    transfer.txData = SpanDataDiscardConst(tx_data);
    transfer.rxData = SpanData(rx_data);
    transfer.dataSize = rx_data.size();
  } else if (!tx_data.empty()) {
    transfer.txData = SpanDataDiscardConst(tx_data);
    transfer.dataSize = tx_data.size();
  } else if (!rx_data.empty()) {
    transfer.rxData = SpanData(rx_data);
    transfer.dataSize = rx_data.size();
  } else {
    return Status::InvalidArgument();
  }

  current_transaction_ = {
      .rx_data = rx_data,
  };

  PW_LOG_DEBUG("Starting a new transaction (%u bytes)", transfer.dataSize);

  if (config_.handle_cs) {
    // Complete the transfer when CS is deasserted.
    SPI_EnableSSInterrupt(base_);
  }

  status_t sdk_status = SPI_SlaveTransferDMA(base_, &handle_, &transfer);
  if (sdk_status != kStatus_Success) {
    PW_LOG_ERROR("SPI_SlaveTransferDMA failed: %ld", sdk_status);
    return ToPwStatus(sdk_status);
  }

  return OkStatus();
}

void McuxpressoResponder::DoCancel() {
  if (!TryChangeState(State::kBusy, State::kIdle)) {
    return;
  }
  TransferComplete(Status::Cancelled(), 0);
}

}  // namespace pw::spi
