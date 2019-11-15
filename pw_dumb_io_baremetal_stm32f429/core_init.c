// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

//                               !!!WARNING!!!
//
// Some of the code in this file is run without static initialization expected
// by C/C++. Any accesses to statically initialized objects/variables before
// memory is initialized will result in undefined values and violates the C
// specification. Only code run after memory initialization is complete will be
// compliant and truly safe to run. In general, make early initialization code
// run AFTER memory initialization has complete unless it is ABSOLUTELY
// NECESSARY to modify the way memory is initialized.
//
// This file is similar to a traditional assembly startup file. It turns out
// that everything typically done in ARMv7-M assembly startup can be done
// straight from C code. This makes startup code easier to maintain, modify,
// and read.
//
// Core initialization is comprised of two primary parts:
//
// 1. Initialize ARMv7-M Vector Table: The ARMv7-M vector table (See ARMv7-M
//    Architecture Reference Manual DDI 0403E.b section B1.5) dictates the
//    starting program counter (PC) and stack pointer (SP) when the SoC powers
//    on. The vector table also contains a number of other vectors to handle
//    different exceptions. This file omits many of the vectors and only
//    configures the four most important ones.
//
// 2. Initialize Memory: When execution begins due to SoC power-on (or the
//    device is reset), memory must be initialized to ensure it contains the
//    expected values when code begins to run. The SoC doesn't inherently have a
//    notion of how to do this, so before ANYTHING else the memory must be
//    initialized. This is done at the beginning of pw_FirmwareInit().
//
//
// The simple flow is as follows:
//   Power on -> PC and SP set (from vector_table by SoC) -> pw_FirmwareInit()
//
// In pw_FirmwareInit():
//   Initialize memory -> initialize board (pre-main init) -> main()

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pw_preprocessor/compiler.h"

// Extern symbols referenced in the vector table.
extern const uint8_t _stack_end[];
extern uint8_t _static_init_ram_start[];
extern uint8_t _static_init_ram_end[];
extern uint8_t _static_init_flash_start[];
extern uint8_t _zero_init_ram_start[];
extern uint8_t _zero_init_ram_end[];

// Functions called as part of firmware initialization.
void __libc_init_array(void);
void pw_BoardInit(void);
int main(void);

void DefaultFaultHandler(void) {
  while (true) {
    // Wait for debugger to attach.
  }
}

// WARNING: This code is run immediately upon boot, and performs initialization
// of RAM. Note that code running before this function finishes memory
// initialization will violate the C spec (Section 6.7.8, paragraph 10 for
// example, which requires uninitialized static values to be zero-initialized).
// Be EXTREMELY careful when running code before this function finishes RAM
// initialization.
//
// This function runs immediately at boot because it is at index 1 of the
// interrupt vector table.
PW_NO_PROLOGUE void pw_FirmwareInit() {
  // Begin memory initialization.
  // Static-init RAM.
  memcpy(_static_init_ram_start,
         _static_init_flash_start,
         _static_init_ram_end - _static_init_ram_start);

  // Zero-init RAM.
  memset(_zero_init_ram_start, 0, _zero_init_ram_end - _zero_init_ram_start);

  // Call static constructors.
  __libc_init_array();

  // End memory initialization.

  // Do any necessary board init.
  pw_BoardInit();

  // Run main.
  main();

  // In case main() returns, just sit here until the device is reset.
  while (true) {
  }
}

// This is the device's interrupt vector table. It's not referenced in any
// code because the platform (STM32F4xx) expects this table to be present at the
// beginning of flash.
//
// For more information, see ARMv7-M Architecture Reference Manual DDI 0403E.b
// section B1.5.3.
PW_KEEP_IN_SECTION(".vector_table")
const uint32_t vector_table[] = {
    // The starting location of the stack pointer.
    [0] = (uint32_t)_stack_end,

    // Reset handler, dictates how to handle reset interrupt. This is also run
    // at boot.
    [1] = (uint32_t)pw_FirmwareInit,

    // NMI handler.
    [2] = (uint32_t)DefaultFaultHandler,
    // HardFault handler.
    [3] = (uint32_t)DefaultFaultHandler,
};
