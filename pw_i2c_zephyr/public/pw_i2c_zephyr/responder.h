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

#pragma once

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#include "pw_function/function.h"
#include "pw_i2c/address.h"
#include "pw_i2c/responder.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::i2c {

/// @module{pw_i2c_zephyr}

/// Implements the pw::i2c::Responder interface using the Zephyr I2C target API.
///
/// This class allows a Pigweed I2C responder implementation to be used with a
/// Zephyr I2C peripheral operating in target mode.
/// It is marked final to ensure CONTAINER_OF remains safe.
class ZephyrResponder final : public Responder {
 public:
  using OnStartReadCallback = pw::Function<Status()>;
  using OnStartWriteCallback = pw::Function<Status()>;
  using OnWriteCallback = pw::Function<Status(ConstByteSpan)>;
  using OnReadCallback = pw::Function<Result<ByteSpan>()>;
  using OnStopCallback = pw::Function<Status()>;

  // Constructs a ZephyrResponder.
  //
  // @param zephyr_i2c_device Pointer to the Zephyr I2C device struct.
  // @param address The I2C address this responder will listen on.
  // @param events Event handler.
  ZephyrResponder(const ::device* zephyr_i2c_device,
                  pw::i2c::Address address,
                  ResponderEvents& events);

  ~ZephyrResponder();

 private:
  /// @name Responder interface overrides
  /// @{
  Status DoEnable() override;
  Status DoDisable() override;
  /// @}

  // Zephyr I2C target callback functions. These are static and use
  // CONTAINER_OF to get the ZephyrResponder instance.
  static int ZephyrWriteRequestedCb(::i2c_target_config* cfg);
  static int ZephyrWriteReceivedCb(::i2c_target_config* cfg, uint8_t val);
  static int ZephyrReadRequestedCb(::i2c_target_config* cfg, uint8_t* val);
  static int ZephyrReadProcessedCb(::i2c_target_config* cfg, uint8_t* val);
  static int ZephyrStopCb(::i2c_target_config* cfg);

#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
  static void ZephyrBufWriteReceivedCb(::i2c_target_config* cfg,
                                       uint8_t* ptr,
                                       uint32_t len);
  static int ZephyrBufReadRequestedCb(::i2c_target_config* cfg,
                                      uint8_t** ptr,
                                      uint32_t* len);
#endif

  const ::device* zephyr_i2c_device_;

  struct ZephyrTarget {
    ZephyrResponder* self;
    ::i2c_target_config config;
    ::i2c_target_callbacks callbacks;
  } zephyr_target_ = {};

  /// Used in non-buffered mode to serve data byte-by-byte for Zephyr.
  /// This span points to data provided by the on_read_cb_.
  ConstByteSpan current_read_data_span_;
};

/// @}

}  // namespace pw::i2c
