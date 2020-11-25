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

// Configuration macros for the pw_rpc module.
#pragma once

#include <cstddef>

// The Nanopb-based pw_rpc implementation allocates memory to use for Nanopb
// structs for the request and response protobufs. The template function that
// allocates these structs rounds struct sizes up to this value so that
// different structs can be allocated with the same function. Structs with sizes
// larger than this value cause an extra function to be created, which slightly
// increases code size.
//
// Ideally, this value will be set to the size of the largest Nanopb struct used
// as an RPC request or response. The buffer can be stack or globally allocated
// (see PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE).
#ifndef PW_RPC_NANOPB_STRUCT_MIN_BUFFER_SIZE
#define PW_RPC_NANOPB_STRUCT_MIN_BUFFER_SIZE 64
#endif  // PW_RPC_NANOPB_STRUCT_MIN_BUFFER_SIZE

namespace pw::rpc::cfg {

inline constexpr size_t kNanopbStructMinBufferSize =
    PW_RPC_NANOPB_STRUCT_MIN_BUFFER_SIZE;

}  // namespace pw::rpc::cfg

#undef PW_RPC_NANOPB_STRUCT_MIN_BUFFER_SIZE

// This option determines whether to allocate the Nanopb structs on the stack or
// in a global variable. Globally allocated structs are NOT thread safe, but
// work fine when the RPC server's ProcessPacket function is only called from
// one thread.
#ifndef PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE
#define PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE 1
#endif  // PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE

#if PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE
#define _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
#else
#define _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS static
#endif  // PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE

#undef PW_RPC_NANOPB_STRUCT_BUFFER_STACK_ALLOCATE
