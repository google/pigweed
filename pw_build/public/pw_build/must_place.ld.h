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

/// @defgroup pw_must_place
/// @{

// clang-format off
/// @cond PRIVATE
#define ___PW_MUST_PLACE(isection, start_sym, end_sym)  \
    start_sym = .;                                              \
    isection                                                    \
    end_sym = .;                                                \
    ASSERT(start_sym != end_sym,                                \
           "Error: No symbols found in pattern below");         \
    ASSERT(start_sym != end_sym, #isection)

#define __PW_MUST_PLACE(isection, sym_prefix, unique)   \
    ___PW_MUST_PLACE(isection, sym_prefix ## start_ ## unique, sym_prefix ## end_ ## unique)

#define _PW_MUST_PLACE(isection, sym_prefix, unique)    \
    __PW_MUST_PLACE(isection, sym_prefix, unique)
/// @endcond

/// PW_MUST_PLACE is a macro intended for use in linker scripts to ensure inputs
/// are non-zero sized.
///
/// Say you want to place a specific object file into a particular section. You
/// can reference it by file path like:
///
/// @code
/// SECTIONS
/// {
///   .special_code
///   {
///     */src/path/libspecial_code.a:*.o
///   }
/// }
/// @endcode
///
/// This works but is fragile as it will silently break if the filename or path
/// changes. Use PW_MUST_PLACE to get a linker assertion if the input is empty.
///
/// @code
/// #include "pw_build/must_place.ld.h"
///
/// SECTIONS
/// {
///   .special_code
///   {
///     PW_MUST_PLACE(*/src/path/libspecial_code.a:*.o)
///   }
/// }
/// @endcode
///
/// If the wildcard match fails PW_MUST_PLACE will generate an error telling you
/// which input had no symbols.
///
/// @code
///   Error: No symbols found in pattern below
///   */src/path/libspecial_code.a:*.o
/// @endcode
///
/// This could be because you had a typo, the path changed, or the symbols were
/// dropped due to linker section garbage collection.In the latter case,
/// you can choose to add ``KEEP()`` around your input to prevent garbage
/// collection.
#define PW_MUST_PLACE(isection)  \
    _PW_MUST_PLACE(isection, __section_place_, __LINE__)

/// @}

/// @defgroup pw_must_not_place
/// @{

// clang-format off
/// @cond PRIVATE
#define ___PW_MUST_NOT_PLACE(isection, start_sym, end_sym)  \
    start_sym = .;                                              \
    isection                                                    \
    end_sym = .;                                                \
    ASSERT(start_sym == end_sym,                                \
           "Error: Symbols found in pattern below but marked as must not place.");         \
    ASSERT(start_sym == end_sym, #isection)

#define __PW_MUST_NOT_PLACE(isection, sym_prefix, unique)   \
    ___PW_MUST_NOT_PLACE(isection, sym_prefix ## start_ ## unique, sym_prefix ## end_ ## unique)

#define _PW_MUST_NOT_PLACE(isection, sym_prefix, unique)    \
    __PW_MUST_NOT_PLACE(isection, sym_prefix, unique)
/// @endcond

/// PW_MUST_NOT_PLACE is a macro intended for use in linker scripts to ensure inputs
/// are _not_ present. It does the opposite of PW_MUST_PLACE.
///
/// This can be used to assert that no data members are added to an object file where
/// there should be none. This is useful to ensure the safety of code that must run
/// before data or bss init.
///
/// @code
/// SECTIONS
/// {
///   .zero_init_ram_early_init
///   {
///      PW_MUST_NOT_PLACE(*/libearly_memory_init.a:*.o(.bss*));
///   }
/// }
/// @endcode
///
/// When adding a new variable to a library marked with this macro it is expected to
/// change to PW_MUST_PLACE
#define PW_MUST_NOT_PLACE(isection)  \
    _PW_MUST_NOT_PLACE(isection, __section_not_place_, __LINE__)

/// @}
