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

// TODO: b/426012010 - Auto-generate this file from the metadata provided
// in //docs/sphinx/module_metadata.json.

// TODO: b/441319784 - Move each module's content into that module's directory.
// E.g. the pw_allocator content should go into //pw_allocator/doxygen.h

// clang-format off

/// @defgroup pw_alignment pw_alignment
/// @brief Natural object alignment, guaranteed. Main docs: https://pigweed.dev/pw_alignment

/// @defgroup pw_allocator pw_allocator
/// @brief Flexible, safe, and measurable memory allocation
/// @details Main docs: [Home](../../pw_allocator/docs.html) |
/// [Guides](../../pw_allocator/guide.html) |
/// [Design & roadmap](../../pw_allocator/design.html) |
/// [Code size analysis](../../pw_allocator/code_size.html)

/// @defgroup pw_allocator_block Blocks
/// @ingroup pw_allocator
/// @brief An allocatable region of memory
/// @details Blocks are the fundamental type managed by several of the
/// @ref pw_allocator_concrete_block

/// @defgroup pw_allocator_block_impl Block implementations
/// @ingroup pw_allocator_block
/// @brief The following combine block mix-ins and provide both the methods
/// they require as well as a concrete representation of the data those methods
/// need

/// @brief Blocks are defined using several stateless “mix-in” interface types
/// @details These provide specific functionality, while deferring the detailed
/// representation of a block to a derived type.

/// @defgroup pw_allocator_block_mixins Block mix-ins
/// @ingroup pw_allocator_block
/// @brief Blocks are defined using several stateless “mix-in” interface types
/// @details These provide specific functionality, while deferring the detailed
/// representation of a block to a derived type.

/// @defgroup pw_allocator_bucket Buckets
/// @ingroup pw_allocator
/// @brief Data structures that track free blocks

/// @defgroup pw_allocator_config Configuration
/// @ingroup pw_allocator
/// @brief Options for controlling block poisoning intervals, validation
/// checks, and deprecation warnings

/// @defgroup pw_allocator_concrete Concrete allocators
/// @ingroup pw_allocator
/// @brief Concrete allocator implementations that provide memory dynamically

/// @defgroup pw_allocator_concrete_block Block allocators
/// @ingroup pw_allocator_concrete

/// @defgroup pw_allocator_core Core interfaces
/// @ingroup pw_allocator
/// @brief Generic allocator interfaces that can be injected into routines that
/// need dynamic memory
/// @details These include `Allocator`, as well as the `Layout` type that is
/// passed to it and the managed smart pointer types, such as `UniquePtr`, that
/// can be returned from it.

/// @defgroup pw_allocator_forwarding Forwarding allocators
/// @ingroup pw_allocator
/// @brief Allocator implementations that don’t allocate memory directly
/// and instead rely on other allocators while providing additional behaviors
/// @details Learn more: [Forwarding allocator
/// concept](../../pw_allocator/design.html#module-pw-allocator-design-forwarding)

/// @defgroup pw_allocator_impl_test_fuzz FuzzTest support
/// @ingroup pw_allocator_impl_test

/// @defgroup pw_allocator_impl Implementation interfaces
/// @ingroup pw_allocator
/// @brief Interfaces for allocator implementers
/// @details These interfaces are intended for allocator implementers, not for
/// `pw_allocator` consumers.

/// @defgroup pw_allocator_impl_size Size reports
/// @ingroup pw_allocator_impl
/// @brief Generate code size reports for allocator implementations

/// @defgroup pw_allocator_impl_test Testing and debugging
/// @ingroup pw_allocator_impl
/// @brief Test utilities for allocator implementers
/// @details These facilitate writing unit tests and fuzz tests for both
/// concrete and forwarding allocator implementations. They are not intended to
/// be used by module consumers.

/// @defgroup pw_allocator_util Utilities
/// @ingroup pw_allocator
/// @brief Helpers for metrics, fragmentation, and buffer management

/// @defgroup pw_analog pw_analog
/// @brief Analog-to-digital converter libraries and utilities. Main docs: https://pigweed.dev/pw_analog

/// @defgroup pw_async pw_async
/// @brief Portable APIs for asynchronous code. Main docs: https://pigweed.dev/pw_async

/// @defgroup pw_async2 pw_async2
/// @brief Cooperative async tasks for embedded.
/// @details Main docs: [Home](../../pw_async2/docs.html) |
/// [Quickstart](../../pw_async2/quickstart.html) |
/// [Codelab](../../pw_async2/codelab.html) |
/// [Guides](../../pw_async2/guides.html) |
/// [Code size analysis](../../pw_async2/code_size.html) |
/// [Coroutines](../../pw_async2/coroutines.html) |
/// [Backends](../../pw_async2/backends.html)

/// @defgroup pw_async2_adapters Pendable adapters
/// @ingroup pw_async2
/// @brief Pendable wrappers and helpers

/// @defgroup pw_async2_alloc Dynamic allocation
/// @ingroup pw_async2
/// @brief Heap allocate space for tasks or functions with `pw::Allocator`

/// @defgroup pw_async2_backends Dispatcher backends
/// @ingroup pw_async2
/// @brief Dispatcher implementation interfaces

/// @defgroup pw_async2_combiners Combining tasks
/// @ingroup pw_async2
/// @brief Helpers for interacting with multiple pendables

/// @defgroup pw_async2_core Core
/// @ingroup pw_async2
/// @brief Core primitives such as tasks, dispatchers, polls, contexts, and wakers
/// @details Learn more: [Core concepts](../../pw_async2/design.html#core-concepts)

/// @defgroup pw_async2_coroutines Coroutines
/// @ingroup pw_async2
/// @brief C++20 coroutine support. Learn more: [Coroutines](../../pw_async2/coroutines.html)

/// @defgroup pw_async2_pendables Built-in pendables
/// @ingroup pw_async2
/// @brief Async operations that can be polled for completion and suspended. Learn more: [The pendable function interface](../../pw_async2/design.html#the-pendable-function-interface)

/// @defgroup pw_async_basic pw_async_basic
/// @brief Main docs: https://pigweed.dev/pw_async_basic

/// @defgroup pw_base64 pw_base64
/// @brief Base64 encoding, decoding, and validating. Main docs: https://pigweed.dev/pw_base64

/// @defgroup pw_bloat pw_bloat
/// @brief Utilities for generating binary size reports. Main docs: https://pigweed.dev/pw_bloat

/// @defgroup pw_bluetooth pw_bluetooth
/// @brief Host-layer Bluetooth Low Energy APIs and utilities. Main docs: https://pigweed.dev/pw_bluetooth

/// @defgroup pw_bluetooth_proxy pw_bluetooth_proxy
/// @brief Lightweight proxy for augmenting Bluetooth functionality. Main docs: https://pigweed.dev/pw_bluetooth_proxy

/// @defgroup pw_bluetooth_sapphire pw_bluetooth_sapphire
/// @brief Battle-tested Bluetooth with rock-solid reliability. Main docs: https://pigweed.dev/pw_bluetooth_sapphire

/// @defgroup pw_build pw_build
/// @brief Integrations for Bazel, GN, and CMake. Main docs: https://pigweed.dev/pw_build

/// @defgroup pw_bytes pw_bytes
/// @brief Utilities for manipulating binary data. Main docs: https://pigweed.dev/pw_bytes

/// @defgroup pw_channel pw_channel
/// @brief Async, zero-copy API for sending and receiving bytes or datagrams. Main docs: https://pigweed.dev/pw_channel

/// @defgroup pw_chre pw_chre
/// @brief Android Context Hub Runtime Environment backend. Main docs: https://pigweed.dev/pw_chre

/// @defgroup pw_chrono pw_chrono
/// @brief Portable std::chrono for constrained embedded devices. Main docs: https://pigweed.dev/pw_chrono

/// @defgroup pw_clock_tree pw_clock_tree
/// @brief Clock tree management for embedded devices. Main docs: https://pigweed.dev/pw_clock_tree

/// @defgroup pw_clock_tree_mcuxpresso pw_clock_tree_mcuxpresso
/// @brief NXP MCUXpresso SDK implementation for pw_clock_tree. Main docs: https://pigweed.dev/pw_clock_tree_mcuxpresso

/// @defgroup pw_containers pw_containers
/// @brief Generic collections of objects for embedded devices: https://pigweed.dev/pw_containers

/// @defgroup pw_crypto pw_crypto
/// @brief Main docs: https://pigweed.dev/pw_crypto

/// @defgroup pw_digital_io pw_digital_io
/// @brief Digital I/O interface. Main docs: https://pigweed.dev/pw_digital_io

/// @defgroup pw_digital_io_mcuxpresso pw_digital_io_mcuxpresso
/// @brief Digital I/O for NXP MCUXpresso. Main docs: https://pigweed.dev/pw_digital_io_mcuxpresso

/// @defgroup pw_display pw_display
/// @brief Graphic display support and framebuffer management. Main docs: https://pigweed.dev/pw_display

/// @defgroup pw_elf pw_elf
/// @brief ELF file support. Main docs: https://pigweed.dev/pw_elf

/// @defgroup pw_function pw_function
/// @brief Embedded-friendly `std::function`: https://pigweed.dev/pw_function

/// @defgroup pw_hdlc pw_hdlc
/// @brief Simple, robust, and efficient serial communication. Main docs: https://pigweed.dev/pw_hdlc

/// @defgroup pw_hex_dump pw_hex_dump
/// @brief Handy hexdump utilities. Main docs: https://pigweed.dev/pw_hex_dump

/// @defgroup pw_i2c pw_i2c
/// @brief Cross-platform I2C API with interactive debugging. Main docs: https://pigweed.dev/pw_i2c

/// @defgroup pw_i2c_linux pw_i2c_linux
/// @brief Linux userspace implementation for pw_i2c. Main docs: https://pigweed.dev/pw_i2c_linux

/// @defgroup pw_interrupt pw_interrupt
/// @brief Main docs: https://pigweed.dev/pw_interrupt

/// @defgroup pw_json pw_json
/// @brief Simple, efficient C++ JSON serialization. Main docs: https://pigweed.dev/pw_json

/// @defgroup pw_kvs pw_kvs
/// @brief Lightweight, persistent key-value store. Main docs: https://pigweed.dev/pw_kvs

/// @defgroup pw_log_string pw_log_string
/// @brief Main docs: https://pigweed.dev/pw_log_string

/// @defgroup pw_log_tokenized pw_log_tokenized
/// @brief Main docs: https://pigweed.dev/pw_log_tokenized

/// @defgroup pw_malloc pw_malloc
/// @brief Replacement interface for standard libc dynamic memory operations. Main docs: https://pigweed.dev/pw_malloc

/// @defgroup pw_multibuf pw_multibuf
/// @brief A buffer API optimized for zero-copy messaging.
/// @details Main docs: [Home](../../pw_multibuf/docs.html) |
/// [Code size analysis](../../pw_multibuf/code_size.html)

/// @defgroup pw_multibuf_v1 Legacy v1 API
/// @ingroup pw_multibuf
/// @brief Interfaces that will eventually be deprecated
/// @details Most users of `pw_multibuf` will start by allocating a `MultiBuf`
/// using a `MultiBufAllocator` class, such as the `SimpleAllocator`.
///
/// A `MultiBuf` consists of a number of `Chunk` instances representing
/// contiguous memory regions. A `Chunk` can be grown or shrunk which allows
/// `MultiBuf` to be grown or shrunk. This allows, for example, lower layers to
/// reserve part of a `MultiBuf` for a header or footer. See `Chunk` for more
/// details.
///
/// `MultiBuf` exposes an `std::byte` iterator interface as well as a `Chunk`
/// iterator available through the `Chunks()` method. It allows extracting a
/// `Chunk` as an RAII-style `OwnedChunk` which manages its own lifetime.

/// @defgroup pw_multibuf_v1_impl Allocator implementation API
/// @ingroup pw_multibuf_v1
/// @details Some users will need to directly implement the `MultiBufAllocator`
/// interface in order to provide allocation out of a particular region,
/// provide particular allocation policy, fix chunks to some size (such as MTU
/// size - header for socket implementations), or specify other custom behavior.
/// These users will also need to understand and implement the
/// `ChunkRegionTracker` API.
///
/// A simple implementation of a `ChunkRegionTracker` is provided, called
/// `HeaderChunkRegionTracker`. It stores its `Chunk` and region metadata in a
/// `Allocator` allocation alongside the data. The allocation process is
/// synchronous, making this class suitable for testing. The allocated region or
/// `Chunk` must not outlive the provided allocator.
///
/// Another `ChunkRegionTracker` specialization is the lightweight
/// `SingleChunkRegionTracker`, which does not rely on `Allocator` and uses the
/// provided memory view to create a single chunk. This is useful when a single
/// `Chunk` is sufficient at no extra overhead. However, the user needs to own
/// the provided memory and know when a new `Chunk` can be requested.

/// @defgroup pw_multibuf_v1_test Test-only features
/// @ingroup pw_multibuf_v1

/// @defgroup pw_multibuf_v2 Experimental v2 API
/// @ingroup pw_multibuf
/// @brief Experimental API that separates out the concern of memory allocation

/// @defgroup pw_numeric pw_numeric
/// @brief Efficient mathematical utilities for embedded. Main docs: https://pigweed.dev/pw_numeric

/// @defgroup pw_perf_test pw_perf_test
/// @brief Micro-benchmarks that are easy to write and run. Main docs: https://pigweed.dev/pw_perf_test

/// @defgroup pw_persistent_ram pw_persistent_ram
/// @brief Main docs: https://pigweed.dev/pw_persistent_ram

/// @defgroup pw_polyfill pw_polyfill
/// @brief Main docs: https://pigweed.dev/pw_polyfill

/// @defgroup pw_preprocessor pw_preprocessor
/// @brief Helpful preprocessor macros. Main docs: https://pigweed.dev/pw_preprocessor

/// @defgroup pw_random pw_random
/// @brief Main docs: https://pigweed.dev/pw_random

/// @defgroup pw_rpc pw_rpc
/// @brief Efficient, low-code-size RPC system for embedded devices
/// @details Main docs: [Home](../../pw_rpc/docs.html) |
/// [Quickstart & guides](../../pw_rpc/guides.html) |
/// [Client, server, and protobuf libraries](../../pw_rpc/libraries.html) |
/// [C++ server and client](../../pw_rpc/cpp.html) |
/// [Python client](../../pw_rpc/py/docs.html) |
/// [TypeScript client](../../pw_rpc/ts/docs.html) |
/// [Nanopb codegen](../../pw_rpc/nanopb/docs.html) |
/// [pw_protobuf codegen](../../pw_rpc/pwpb/docs.html) |
/// [Packet protocol](../../pw_rpc/protocol.html) |
/// [Design & roadmap](../../pw_rpc/design.html) |
/// [HDLC example](../../pw_hdlc/rpc_example/docs.html)

/// @defgroup pw_rpc_test Benchmarking & testing
/// @ingroup pw_rpc

/// @defgroup pw_rpc_channel Channels
/// @ingroup pw_rpc

/// @defgroup pw_rpc_sync Synchronous API
/// @ingroup pw_rpc

/// @defgroup pw_rpc_config Configuration
/// @ingroup pw_rpc

/// @defgroup pw_span pw_span
/// @brief std::span for C++17. Main docs: https://pigweed.dev/pw_span

/// @defgroup pw_stream pw_stream
/// @brief A foundational interface for streaming data
/// @details Main docs: [Home](../../pw_stream/docs.html) |
/// [Backends](../../pw_stream/backends.html) |
/// [Python](../../pw_stream/py/docs.html)

/// @defgroup pw_stream_interface Interfaces
/// @ingroup pw_stream
/// @brief Generic stream interfaces that support a combination of reading,
/// writing, and seeking

/// @defgroup pw_stream_interface_reader Readers
/// @ingroup pw_stream_interface
/// @brief Streams that support reading but not writing

/// @defgroup pw_stream_interface_writer Writers
/// @ingroup pw_stream_interface
/// @brief Streams that support writing but not reading

/// @defgroup pw_stream_interface_readerwriter ReaderWriters
/// @ingroup pw_stream_interface
/// @brief Streams that support both reading and writing

/// @defgroup pw_stream_concrete Implementations
/// @ingroup pw_stream
/// @brief Concrete implementations of stream interfaces for general use

/// @defgroup pw_stream_uart_linux pw_stream_uart_linux
/// @brief Main docs: https://pigweed.dev/pw_stream_uart_linux

/// @defgroup pw_string pw_string
/// @brief Efficient, easy, and safe string manipulation
/// @details Main docs: [Home](../../pw_string/docs.html) |
/// [Get started & guides](../../pw_string/guide.html) |
/// [Design & roadmap](../../pw_string/design.html) |
/// [Code size analysis](../../pw_string/code_size.html)

/// @defgroup pw_string_inline InlineString and InlineBasicString
/// @ingroup pw_string
/// @brief Safer alternatives to `std::string` and `std::basic_string`

/// @defgroup pw_string_builder StringBuilder
/// @ingroup pw_string
/// @brief The flexibility of `std::ostringstream` but with a small footprint

/// @defgroup pw_string_util Utilities
/// @ingroup pw_string
/// @brief Safer alternatives to C++ standard library string functions

/// @defgroup pw_string_utf8 UTF-8 helpers
/// @ingroup pw_string
/// @brief Basic helpers for reading and writing UTF-8-encoded strings

/// @defgroup pw_sync pw_sync
/// @brief Main docs: [Home](../../pw_sync/docs.html)

/// @defgroup pw_sys_io pw_sys_io
/// @brief Main docs: https://pigweed.dev/pw_sys_io

/// @defgroup pw_system pw_system
/// @brief Main docs: https://pigweed.dev/pw_system

/// @defgroup pw_thread pw_thread
/// @brief Main docs: https://pigweed.dev/pw_thread

/// @defgroup pw_tokenizer pw_tokenizer
/// @brief Compress strings to shrink logs by +75%
/// @details Main docs: [Home](../../pw_tokenizer/docs.html) |
/// [Quickstart](../../pw_tokenizer/get_started.html) |
/// [Tokenization](../../pw_tokenizer/tokenization.html) |
/// [Token databases](../../pw_tokenizer/token_databases.html) |
/// [Detokenization](../../pw_tokenizer/detokenization.html)

/// @defgroup pw_tokenizer_tokenize Tokenization
/// @ingroup pw_tokenizer
/// @brief Convert string literals to tokens

/// @defgroup pw_tokenizer_detokenize Detokenization
/// @ingroup pw_tokenizer
/// @brief Expand a token to the string it represents and decode its arguments

/// @defgroup pw_tokenizer_database Token databases
/// @ingroup pw_tokenizer
/// @brief Store a mapping of tokens to the strings they represent

/// @defgroup pw_tokenizer_config Configuration
/// @ingroup pw_tokenizer
/// @brief Tokenization customization options

/// @defgroup pw_toolchain pw_toolchain
/// @brief Embedded toolchains for GN-based Pigweed projects. Main docs: https://pigweed.dev/pw_toolchain

/// @defgroup pw_trace_tokenized pw_trace_tokenized
/// @brief Main docs: https://pigweed.dev/pw_trace_tokenized

/// @defgroup pw_transfer pw_transfer
/// @brief Main docs: https://pigweed.dev/pw_transfer

/// @defgroup pw_varint pw_varint
/// @brief Functions for encoding and decoding variable length integers. Main docs: https://pigweed.dev/pw_varint

/// @defgroup pw_uart pw_uart
/// @brief Core interfaces for UART communication. Main docs: https://pigweed.dev/pw_uart

/// @defgroup pw_uuid pw_uuid
/// @brief 128-bit universally unique identifier (UUID). Main docs: https://pigweed.dev/pw_uuid

/// @defgroup pw_work_queue pw_work_queue
/// @brief Main docs: https://pigweed.dev/pw_work_queue

/// @defgroup third_party third-party
/// @brief API integrations with third-party software e.g. FreeRTOS

/// @defgroup third_party_freertos FreeRTOS
/// @ingroup third_party
/// @brief FreeRTOS application functions

// clang-format on
