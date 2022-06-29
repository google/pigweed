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

// std::span is a stand-in for C++20's std::span. Do NOT include this header
// directly; instead, include it as <span>.
//
// A span is a non-owning array view class. It refers to an external array by
// storing a pointer and length. Unlike std::array, the size does not have to be
// a template parameter, so this class can be used to without stating its size.
//
// This file a modified version of base::span from Chromium:
//   https://chromium.googlesource.com/chromium/src/+/d93ae920e4309682deb9352a4637cfc2941c1d1f/base/containers/span.h
//
// In order to minimize changes from the original, this file does NOT fully
// adhere to Pigweed's style guide.
//
// A few changes were made to the Chromium version of span. These include:
//   - Use std::data and std::size instead of base::* versions.
//   - Rename base namespace to std.
//   - Rename internal namespace to pw_span_internal.
//   - Remove uses of checked_iterators.h and CHECK.
//   - Replace make_span functions with C++17 class template deduction guides.
//   - Use std::byte instead of uint8_t for compatibility with std::span.
//
#pragma once

#include "pw_polyfill/standard_library/namespace.h"

#ifndef __cpp_lib_span

// When polyfilling std::span, set __cpp_lib_span to the standard value of
// 202002L. When std::span is not polyfilled, this header should not be used. To
// support testing non-polyfilled std::span alongside pw::span, set
// __cpp_lib_span to 202001L (lower than the standard value) so that pw::span is
// not aliased to std::span.
#ifdef _PW_SPAN_POLYFILL_ENABLED
#undef _PW_SPAN_POLYFILL_ENABLED
#define __cpp_lib_span 202002L
#else
#define __cpp_lib_span 202001L
#endif  // _PW_SPAN_POLYFILL_ENABLED

#define _PW_SPAN_COMMON_NAMEPACE_BEGIN _PW_POLYFILL_BEGIN_NAMESPACE_STD
#define _PW_SPAN_COMMON_NAMEPACE_END _PW_POLYFILL_END_NAMESPACE_STD

#include "pw_span/internal/span_common.inc"

#undef _PW_SPAN_COMMON_NAMEPACE_BEGIN
#undef _PW_SPAN_COMMON_NAMEPACE_END

#endif  // __cpp_lib_span
