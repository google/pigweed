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
#pragma once

#include "fsl_usart.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"

namespace pw::stream {

// Stream to output to a UART that is safe to use in an interrupt or fault
// handler context.
class InterruptSafeUartWriterMcuxpresso : public pw::stream::NonSeekableWriter {
 public:
  // Make sure constructor is constexpr. When used with `constinit`, this
  // ensures that the object is fully constructed prior to static constructors
  // having been executed.
  // Doing so requires passing in the uart base pointer as a uintptr_t instead
  // of USART_Type* to avoid needing a reinterpret_cast.
  constexpr InterruptSafeUartWriterMcuxpresso(uintptr_t base,
                                              clock_name_t clock_name,
                                              unsigned int baudrate)
      : base_(base), baudrate_(baudrate), clock_name_(clock_name) {}

  // Initialize uart to known good state. Can be used on UART that was already
  // used by another driver to enable use in fault handler context.
  pw::Status Enable();

 private:
  pw::Status DoWrite(pw::ConstByteSpan data) override;
  USART_Type* base() { return reinterpret_cast<USART_Type*>(base_); }

  const uintptr_t base_;
  const unsigned int baudrate_;
  const clock_name_t clock_name_;
};

}  // namespace pw::stream
