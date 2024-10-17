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

#include "pw_tokenizer/tokenize.h"

namespace pw::tokenizer {

template <typename T>
constexpr bool ValidEnumerator(T) {
  static_assert(std::is_enum_v<T>, "Must be an enum");
  static_assert(sizeof(T) <= sizeof(Token), "Must fit in a token");
  return true;
}

}  // namespace pw::tokenizer

// Declares an individual tokenized enum value.
#define _PW_TOKENIZE_ENUMERATOR(name, enumerator)                          \
  static_assert(                                                           \
      #name[0] == ':' && #name[1] == ':',                                  \
      "Enum names must be fully qualified and start with ::, but \"" #name \
      "\" does not");                                                      \
  static_assert(::pw::tokenizer::ValidEnumerator(name::enumerator));       \
  PW_TOKENIZER_DEFINE_TOKEN(                                               \
      static_cast<::pw::tokenizer::Token>(name::enumerator),               \
      #name,                                                               \
      #enumerator)

#define _PW_TOKENIZE_ENUM_2(fully_qualified_name, arg1)           \
  _PW_TOKENIZE_ENUMERATOR(fully_qualified_name, arg1);            \
  [[maybe_unused]] constexpr const char* PwTokenizerEnumToString( \
      fully_qualified_name _pw_enum_value) {                      \
    switch (_pw_enum_value) {                                     \
      case fully_qualified_name::arg1:                            \
        return #arg1;                                             \
    }                                                             \
    return "Unknown " #fully_qualified_name " value";             \
  }                                                               \
  static_assert(true);

#define _PW_TOKENIZE_ENUM_3(fully_qualified_name, arg1, arg2)     \
  _PW_TOKENIZE_ENUMERATOR(fully_qualified_name, arg1);            \
  _PW_TOKENIZE_ENUMERATOR(fully_qualified_name, arg2);            \
  [[maybe_unused]] constexpr const char* PwTokenizerEnumToString( \
      fully_qualified_name _pw_enum_value) {                      \
    switch (_pw_enum_value) {                                     \
      case fully_qualified_name::arg1:                            \
        return #arg1;                                             \
      case fully_qualified_name::arg2:                            \
        return #arg2;                                             \
    }                                                             \
    return "Unknown " #fully_qualified_name " value";             \
  }                                                               \
  static_assert(true);

#define _PW_TOKENIZE_ENUM_4(fully_qualified_name, arg1, arg2, arg3) \
  _PW_TOKENIZE_ENUMERATOR(fully_qualified_name, arg1);              \
  _PW_TOKENIZE_ENUMERATOR(fully_qualified_name, arg2);              \
  _PW_TOKENIZE_ENUMERATOR(fully_qualified_name, arg3);              \
  [[maybe_unused]] constexpr const char* PwTokenizerEnumToString(   \
      fully_qualified_name _pw_enum_value) {                        \
    switch (_pw_enum_value) {                                       \
      case fully_qualified_name::arg1:                              \
        return #arg1;                                               \
      case fully_qualified_name::arg2:                              \
        return #arg2;                                               \
      case fully_qualified_name::arg3:                              \
        return #arg3;                                               \
    }                                                               \
    return "Unknown " #fully_qualified_name " value";               \
  }                                                                 \
  static_assert(true);
