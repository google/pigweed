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

.cfi_sections .debug_frame
.section .text.entrypoint, "ax", %progbits
.global  _start
.type    _start, %function
.thumb_func
.cfi_startproc

_start:
	// Static-init RAM (load static values into RAM, .data section init).
	// RAM is 4 byte aligned.
	ldr  r0, =_pw_static_init_ram_start   // destination (RAM)
	ldr  r1, =_pw_static_init_flash_start // source (Flash)
	ldr  r2, =_pw_static_init_ram_end     // end destination (RAM)
	subs r2, r2, r0                       // number of bytes to memcpy
	bl   memcpy

	// Zero-init RAM (.bss section init).
	ldr  r0, =_pw_zero_init_ram_start // destination (RAM)
	movs r1, #0                       // value to write
	ldr  r2, =_pw_zero_init_ram_end   // end destination (RAM)
	subs r2, r2, r0                   // number of bytes to memset
	bl   memset

	// Memory is initialized, now call the applications main entry function.
	bl main

	// Trap if main returns, which it never should.
	udf   #1
	.cfi_endproc
	.size _start, . - _start
