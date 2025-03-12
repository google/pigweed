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

#ifndef PW_ALLOCATOR_BLOCK_POISON_INTERVAL
/// Controls how frequently blocks are poisoned on deallocation.
///
/// Blocks may be "poisoned" when deallocated by writing a pattern to their
/// useable memory space. When next allocated, the pattern is checked to ensure
/// it is unmodified, i.e. that nothing has changed the memory while it was
/// free. If the memory has been changed, then a heap-overflow, use-after-free,
/// or other memory corruption bug exists and the program aborts.
///
/// If set to 0, poisoning is disabled. For any other value N, every Nth block
/// is poisoned. This allows consumers to stochiastically sample allocations for
/// memory corruptions while mitigating the performance impact.
#define PW_ALLOCATOR_BLOCK_POISON_INTERVAL 0
#endif  // PW_ALLOCATOR_BLOCK_POISON_INTERVAL

/// Applies essential checks only.
///
/// This is a possible value for ``PW_ALLOCATOR_HARDENING``.
///
/// Essential checks include those that should almost never be disabled. An
/// example is input validation on the public API, e.g. checking if a pointer
/// passed to ``Allocator::Deallocate`` ]refers to valid allocation.
#define PW_ALLOCATOR_HARDENING_BASIC 1

/// Applies recommended and essential checks.
///
/// This is a possible value for ``PW_ALLOCATOR_HARDENING``.
///
/// Recommended checks include those that can detect memory corruption. These
/// can be very useful in uncovering software defects in other components and in
/// preventing some security vulnerabilities. As a result, disabling these
/// checks is discouraged for all projects except those that have strict size
/// requirements and very high confidence in their codebase.
///
/// See ``PW_ALLOCATOR_HARDENING_BASIC`` for a description of essential checks.
#define PW_ALLOCATOR_HARDENING_ROBUST 2

/// Applies all checks.
///
/// This is a possible value for ``PW_ALLOCATOR_HARDENING``.
///
/// Debug checks include those that check invariants whose failure indicates a
/// defect in pw_allocator itself. For example, allocating a new block from an
/// existing valid free block should result in both blocks being valid with
/// consistent sizes and pointers to neighbors.
///
/// See ``PW_ALLOCATOR_HARDENING_BASIC`` for a description of essential checks.
/// See ``PW_ALLOCATOR_HARDENING_ROBUST`` for a description of recommended
/// checks.
#define PW_ALLOCATOR_HARDENING_DEBUG 3

/// Enables validation checks.
///
/// Possible values are:
///
/// - PW_ALLOCATOR_HARDENING_BASIC
/// - PW_ALLOCATOR_HARDENING_ROBUST (default)
/// - PW_ALLOCATOR_HARDENING_DEBUG
///
/// See those values for a description of the types of checks associated with
/// each level. Subsequent levels include the former, i.e. 'debug' includes
/// 'robust', which includes 'basic'. Additional checks can detect more errors
/// at the cost of performance and code size.
#ifndef PW_ALLOCATOR_HARDENING
#define PW_ALLOCATOR_HARDENING PW_ALLOCATOR_HARDENING_ROBUST
#endif  // PW_ALLOCATOR_HARDENING

#ifndef PW_ALLOCATOR_SUPPRESS_DEPRECATED_WARNINGS
/// Suppresses warnings about using legacy allocator interfaces.
///
/// This module is undergoing refactoring to improve flexibility and
/// performance. Some portions of the API that are being updated are in use by
/// downstream consumers. These legacy interfaces are preserved for now, but
/// deprecated.
///
/// Initially, this setting defaults to 1 and these interfaces may still be
/// consumed without warning.  At some point, this will default to 0. Downstream
/// projects may still suppress the warning by overriding this configuration,
/// but must be aware that legacy interfaces will eventually be removed.
///
/// See b/376730645 for background and details.
#define PW_ALLOCATOR_SUPPRESS_DEPRECATED_WARNINGS 1
#endif  // PW_ALLOCATOR_SUPPRESS_DEPRECATED_WARNINGS

#if PW_ALLOCATOR_SUPPRESS_DEPRECATED_WARNINGS
#define PW_ALLOCATOR_DEPRECATED
#else
#define PW_ALLOCATOR_DEPRECATED \
  [[deprecated("See b/376730645 for background and workarounds.")]]
#endif  // PW_ALLOCATOR_SUPPRESS_DEPRECATED_WARNINGS

/// Indicates whether to include code that requires atomic support.
///
/// As an example, the `ControlBlock` used by both `SharedPtr` and `WeakPtr`
/// needs an `atomic_uint32_t`.
///
/// This defaults to atomics being available.
///
/// TODO(b/402489948): Remove when portable atomics are provided by `pw_atomic`.
#ifndef PW_ALLOCATOR_HAS_ATOMICS
#define PW_ALLOCATOR_HAS_ATOMICS 1
#endif  // PW_ALLOCATOR_HAS_ATOMICS
