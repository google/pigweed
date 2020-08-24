// Copyright 2020 The Pigweed Authors
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

//                               !!!WARNING!!!
//
// Some of the code in this file is run without static initialization expected
// by C/C++. Any accesses to statically initialized objects/variables before
// memory is initialized will result in undefined values and violates the C
// specification. Only code run after memory initialization is complete will be
// compliant and truly safe to run. In general, make early initialization code
// run AFTER memory initialization has completed unless it is ABSOLUTELY
// NECESSARY to modify the way memory is initialized.
//
// This file is similar to a traditional assembly startup file. It turns out
// that everything typically done in ARMv7-M assembly startup can be done
// straight from C code. This makes startup code easier to maintain, modify,
// and read.
//
// When execution begins due to SoC power-on (or the device is reset), three
// key things must happen to properly enter C++ execution context:
//   1. Static variables must be loaded from flash to RAM.
//   2. Zero-initialized variables must be zero-initialized.
//   3. Statically allocated objects must have their constructors run.
// The SoC doesn't inherently have a notion of how to do this, so this is
// handled in StaticInit();
//
// Following this, execution is handed over to pw_PreMainInit() to facilitate
// platform, project, or application pre-main initialization. When
// pw_PreMainInit() returns, main() is executed.
//
// The simple flow is as follows:
//   1. Power on
//   2. PC and SP set (from vector_table by SoC, or by bootloader)
//   3. pw_boot_Entry()
//     3.1. pw_boot_PreStaticMemoryInit();
//     3.2. Static-init memory (.data, .bss)
//     3.3. pw_boot_PreStaticConstructorInit();
//     3.4. Static C++ constructors
//     3.5. pw_boot_PreMainInit()
//     3.6. main()
//     3.7. pw_boot_PostMain()

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "pw_boot_armv7m/boot.h"
#include "pw_preprocessor/compiler.h"

// Extern symbols provided by linker script.
// These symbols tell us where various memory sections start and end.
extern uint8_t _pw_static_init_ram_start;
extern uint8_t _pw_static_init_ram_end;
extern uint8_t _pw_static_init_flash_start;
extern uint8_t _pw_zero_init_ram_start;
extern uint8_t _pw_zero_init_ram_end;

// Functions called as part of firmware initialization.
void __libc_init_array(void);

// WARNING: Be EXTREMELY careful when running code before this function
// completes. The context before this function violates the C spec
// (Section 6.7.8, paragraph 10 for example, which requires uninitialized static
// values to be zero-initialized).
void StaticMemoryInit(void) {
  // Static-init RAM (load static values into ram, .data section init).
  memcpy(&_pw_static_init_ram_start,
         &_pw_static_init_flash_start,
         &_pw_static_init_ram_end - &_pw_static_init_ram_start);

  // Zero-init RAM (.bss section init).
  memset(&_pw_zero_init_ram_start,
         0,
         &_pw_zero_init_ram_end - &_pw_zero_init_ram_start);
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
void pw_boot_Entry() {
  // Run any init that must be done before static init of RAM which preps the
  // .data (static values not yet loaded into ram) and .bss sections (not yet
  // zero-initialized).
  pw_boot_PreStaticMemoryInit();

  // Note that code running before this function finishes memory
  // initialization will violate the C spec (Section 6.7.8, paragraph 10 for
  // example, which requires uninitialized static values to be
  // zero-initialized). Be EXTREMELY careful when running code before this
  // function finishes static memory initialization.
  StaticMemoryInit();

  // Run any init that must be done before C++ static constructors.
  pw_boot_PreStaticConstructorInit();

  // Call static constructors.
  __libc_init_array();

  // This function is not provided by pw_boot_armv7m, a platform layer, project,
  // or application is expected to implement it.
  pw_boot_PreMainInit();

  // Run main.
  main();

  // In case main() returns, invoke this hook.
  pw_boot_PostMain();

  PW_UNREACHABLE;
}
