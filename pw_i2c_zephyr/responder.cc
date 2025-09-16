// Copyright 2025 The Pigweed Authors
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

#define PW_LOG_MODULE_NAME "pw_i2c_zephyr"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "pw_i2c_zephyr/responder.h"

#include <zephyr/sys/util.h>  // For CONTAINER_OF

namespace pw::i2c {

ZephyrResponder::ZephyrResponder(const ::device* zephyr_i2c_device,
                                 pw::i2c::Address address,
                                 ResponderEvents& events)
    : Responder(address, events), zephyr_i2c_device_(zephyr_i2c_device) {
  zephyr_target_.self = this;
  zephyr_target_.callbacks.write_requested = ZephyrWriteRequestedCb;
  zephyr_target_.callbacks.write_received = ZephyrWriteReceivedCb;
  zephyr_target_.callbacks.read_requested = ZephyrReadRequestedCb;
  zephyr_target_.callbacks.read_processed = ZephyrReadProcessedCb;
  zephyr_target_.callbacks.stop = ZephyrStopCb;

#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
  PW_LOG_DEBUG("I2C_TARGET_BUFFER_MODE is enabled.");
  zephyr_target_.callbacks.buf_write_received = ZephyrBufWriteReceivedCb;
  zephyr_target_.callbacks.buf_read_requested = ZephyrBufReadRequestedCb;
#else
  PW_LOG_DEBUG("I2C_TARGET_BUFFER_MODE is not enabled.");
#endif

  zephyr_target_.config.callbacks = &zephyr_target_.callbacks;
  zephyr_target_.config.address = this->address().GetAddress();
  if (this->address().IsTenBit()) {
    zephyr_target_.config.flags = I2C_TARGET_FLAGS_ADDR_10_BITS;
  } else {
    zephyr_target_.config.flags = 0;
  }
  // The `node` field of i2c_target_config is for the driver's internal use.
}

ZephyrResponder::~ZephyrResponder() { Disable().IgnoreError(); }

pw::Status ZephyrResponder::DoEnable() {
  if (!device_is_ready(zephyr_i2c_device_)) {
    PW_LOG_ERROR("Zephyr I2C device %s is not ready.",
                 zephyr_i2c_device_->name);
    return pw::Status::Unavailable();
  }
  int ret = i2c_target_register(zephyr_i2c_device_, &zephyr_target_.config);
  if (ret != 0) {
    PW_LOG_ERROR("Failed to register I2C target (address 0x%02X): %d",
                 address().GetAddress(),
                 ret);
    return pw::Status::Internal();
  }
  PW_LOG_INFO("Zephyr I2C responder registered at address 0x%02X",
              address().GetAddress());
  return pw::OkStatus();
}

pw::Status ZephyrResponder::DoDisable() {
  if (!device_is_ready(zephyr_i2c_device_)) {  // Check if device was ever ready
    return pw::Status::FailedPrecondition();
  }
  int ret = i2c_target_unregister(zephyr_i2c_device_, &zephyr_target_.config);
  if (ret != 0) {
    PW_LOG_ERROR("Failed to unregister I2C target (address 0x%02X): %d",
                 address().GetAddress(),
                 ret);
    return pw::Status::Internal();
  }
  PW_LOG_INFO("Zephyr I2C responder unregistered for address 0x%02X",
              address().GetAddress());

  return pw::OkStatus();
}

// --- Static Callback Implementations ---

int ZephyrResponder::ZephyrWriteRequestedCb(::i2c_target_config* cfg) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;
  bool success = self->OnStartWrite();
  if (!success) {
    PW_LOG_WARN("OnStartWrite failed. NACKing.");
    return -EIO;  // Causes NACK
  }
  PW_LOG_DEBUG("WriteRequested: ACK");
  return 0;  // ACK
}

int ZephyrResponder::ZephyrWriteReceivedCb(::i2c_target_config* cfg,
                                           uint8_t val) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;

  std::byte byte_val = static_cast<std::byte>(val);
  ConstByteSpan data_span(&byte_val, 1);
  PW_LOG_DEBUG("WriteReceived: passing byte 0x%02X to OnWrite.", val);
  bool success = self->OnWrite(data_span);
  if (success) {
    PW_LOG_DEBUG("OnWrite for byte 0x%02X successful. ACK.", val);
    return 0;  // ACK
  }

  PW_LOG_WARN("OnWrite for byte 0x%02X failed. NACKing.", val);
  return -EIO;  // NACK
}

int ZephyrResponder::ZephyrReadRequestedCb(::i2c_target_config* cfg,
                                           uint8_t* val) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;
  bool success = self->OnStartRead();
  if (!success) {
    PW_LOG_WARN("OnStartRead failed");
    return -EIO;  // Error to controller
  }

  // Get data from the application via OnRead
  Result<ConstByteSpan> result = self->OnRead();
  if (!result.ok()) {
    PW_LOG_WARN("OnRead failed: %u.", result.status().code());
    self->current_read_data_span_ = ConstByteSpan();
    return -EAGAIN;  // Error to controller
  }
  self->current_read_data_span_ = result.value();

  if (self->current_read_data_span_.empty()) {
    PW_LOG_DEBUG("No data available from OnRead for ReadRequested.");
    return -ENODATA;
  }

  *val = static_cast<uint8_t>(self->current_read_data_span_[0]);
  self->current_read_data_span_ = self->current_read_data_span_.subspan(1);
  return 0;
}

int ZephyrResponder::ZephyrReadProcessedCb(::i2c_target_config* cfg,
                                           uint8_t* val) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;

  if (self->current_read_data_span_.empty()) {
    // Current span exhausted, try to get more data
    Result<ConstByteSpan> result = self->OnRead();
    if (!result.ok()) {
      PW_LOG_WARN("OnRead failed in ReadProcessed: %u.",
                  result.status().code());
      self->current_read_data_span_ = ConstByteSpan();
      return -EAGAIN;  // Error to controller
    }
    self->current_read_data_span_ = result.value();
  }

  if (self->current_read_data_span_.empty()) {
    PW_LOG_DEBUG("No data available from OnRead for ReadProcessed.");
    return -ENODATA;
  }

  *val = static_cast<uint8_t>(self->current_read_data_span_[0]);
  self->current_read_data_span_ = self->current_read_data_span_.subspan(1);
  return 0;
}

int ZephyrResponder::ZephyrStopCb(::i2c_target_config* cfg) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;
  PW_LOG_DEBUG("I2C Stop condition received.");

  // Call the OnStop implementation, which will call the user's callback.
  // State (in_read_transaction_, etc.) is reset within OnStop().
  if (self->OnStop()) {
    return 0;
  }
  return -EIO;
}

#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
void ZephyrResponder::ZephyrBufWriteReceivedCb(::i2c_target_config* cfg,
                                               uint8_t* ptr,
                                               uint32_t len) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;
  PW_LOG_DEBUG("BufWriteReceived: %u bytes at %p", len, ptr);

  if (!self->OnStartWrite()) {
    PW_LOG_WARN("OnStartWrite failed in BufWriteReceived. Data might be lost.");
    // Callback is void, can't signal error directly to I2C bus here.
    return;
  }
  self->OnWrite(ConstByteSpan(reinterpret_cast<const std::byte*>(ptr), len));
}

int ZephyrResponder::ZephyrBufReadRequestedCb(::i2c_target_config* cfg,
                                              uint8_t** ptr,
                                              uint32_t* len) {
  ZephyrResponder* self = CONTAINER_OF(cfg, ZephyrTarget, config)->self;
  PW_LOG_DEBUG("BufReadRequested");

  if (!self->OnStartRead()) {
    PW_LOG_WARN("OnStartRead failed in BufReadRequested");
    *len = 0;
    return -EIO;  // Error to controller
  }

  Result<ConstByteSpan> result = self->OnRead();
  if (!result.ok()) {
    PW_LOG_WARN("OnRead failed in BufReadRequested: %u",
                result.status().code());
    *len = 0;
    return -EIO;  // Error to controller
  }

  ConstByteSpan data_to_send = result.value();
  if (data_to_send.empty()) {
    PW_LOG_DEBUG("OnRead returned empty span in BufReadRequested.");
    *len = 0;
    return -ENODATA;
  }

  // DANGER: The callback provided to ZephyrResponder (on_read_cb_) must ensure
  // the lifetime of the data pointed to by data_to_send.data() until the
  // transaction is complete (e.g., until STOP or next start).
  *ptr = const_cast<uint8_t*>(
      reinterpret_cast<const uint8_t*>(data_to_send.data()));
  *len = data_to_send.size();
  PW_LOG_DEBUG(
      "BufReadRequested: Providing %u bytes at %p directly from OnRead span",
      *len,
      *ptr);
  return 0;  // Success
}
#endif  // CONFIG_I2C_TARGET_BUFFER_MODE

}  // namespace pw::i2c
