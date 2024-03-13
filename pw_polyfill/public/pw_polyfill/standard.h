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
#pragma once

#if defined(__cplusplus)

/// Evaluates true if the provided C++ standard (98, 11, 14, 17, 20, 23) is
/// supported by the compiler. This is a simple alternative to checking the
/// value of the `__cplusplus` macro.
#define PW_CXX_STANDARD_IS_SUPPORTED(std) \
  (__cplusplus >= _PW_CXX_STANDARD_##std())

/// Evaluates true if the provided C standard (89, 98, 11, 17, 23) is supported
/// by the compiler. This is a simple alternative to checking the value of the
/// `__STDC_VERSION__` macro.
#define PW_C_STANDARD_IS_SUPPORTED(std) (0 >= _PW_C_STANDARD_##std())

#elif defined(__STDC_VERSION__)

#define PW_CXX_STANDARD_IS_SUPPORTED(std) (0 >= _PW_CXX_STANDARD_##std())

#define PW_C_STANDARD_IS_SUPPORTED(std) \
  (__STDC_VERSION__ >= _PW_C_STANDARD_##std())

#endif  // defined(__cplusplus)

// Standard values of __cplusplus and __STDC_VERSION__. See the GCC docs for
// more info: https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
#define _PW_CXX_STANDARD_98() 199711L
#define _PW_CXX_STANDARD_11() 201103L
#define _PW_CXX_STANDARD_14() 201402L
#define _PW_CXX_STANDARD_17() 201703L
#define _PW_CXX_STANDARD_20() 202002L
#define _PW_CXX_STANDARD_23() 202302L

#define _PW_C_STANDARD_89() 199409L
#define _PW_C_STANDARD_99() 199901L
#define _PW_C_STANDARD_11() 201112L
#define _PW_C_STANDARD_17() 201710L
#define _PW_C_STANDARD_23() 202311L
