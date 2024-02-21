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

#include "pw_json/builder.h"
#include "pw_string/type_to_string.h"

namespace pw {
namespace json_impl {

constexpr char kArray[2] = {'[', ']'};
constexpr char kObject[2] = {'{', '}'};

constexpr StatusWithSize WriteString(std::string_view value,
                                     char* buffer,
                                     size_t remaining) {
  if (value.size() + 1 /* null */ > remaining) {
    return StatusWithSize::ResourceExhausted();
  }
  for (char c : value) {
    *buffer++ = c;
  }
  *buffer = '\0';
  return StatusWithSize(value.size());
}

constexpr char NibbleToHex(uint8_t nibble) {
  return nibble + (nibble < 10 ? '0' : ('a' - 10));
}

// In accordance with RFC 8259, JSON strings must escape control characters,
// quotation marks, and reverse solidus (\). This function copies a string and
// escapes these characters as shown in http://www.json.org/string.gif.
//
// The return value is the number of bytes written to the destination
// buffer or -1 if the string does not fit in the destination buffer. Since
// escaped characters result in two or six bytes in the output, the return value
// won't necessarily equal the number of bytes read from the source buffer.
//
// The destination buffer is NEVER null-terminated!
//
// Currently this function ONLY supports ASCII! Bytes ≥128 will be escaped
// individually, rather than treated as multibyte Unicode characters.
constexpr int EscapedStringCopy(char* destination,
                                int copy_limit,
                                std::string_view source) {
  int destination_index = 0;

  for (char source_char : source) {
    if (destination_index >= copy_limit) {
      return -1;
    }

    char escaped_character = '\0';

    if (source_char >= '\b' && source_char <= '\r' && source_char != '\v') {
      constexpr char kControlChars[] = {'b', 't', 'n', '?', 'f', 'r'};
      escaped_character = kControlChars[source_char - '\b'];
    } else if (source_char == '"' || source_char == '\\') {
      escaped_character = source_char;
    } else if (source_char >= ' ' && source_char <= '~') {
      // This is a printable character; no escaping is needed.
      destination[destination_index++] = source_char;
      continue;  // Skip the escaping step below.
    } else {
      // Escape control characters that haven't already been handled. These take
      // six bytes to encode (e.g. \u0056).
      if (copy_limit - destination_index < 6) {
        return -1;
      }

      destination[destination_index++] = '\\';
      destination[destination_index++] = 'u';
      destination[destination_index++] = '0';  // Only handle ASCII for now
      destination[destination_index++] = '0';
      destination[destination_index++] = NibbleToHex((source_char >> 4) & 0x0f);
      destination[destination_index++] = NibbleToHex(source_char & 0x0f);
      continue;  // Already escaped; skip the single-character escaping step.
    }

    // Escape the \b, \t, \n, \f, \r, \", or \\ character, if it fits.
    if (copy_limit - destination_index < 2) {
      return -1;
    }

    destination[destination_index++] = '\\';
    destination[destination_index++] = escaped_character;
  }
  return destination_index;
}

// Writes "<value>", escaping special characters; returns true if successful.
// Null terminates ONLY if successful.
constexpr StatusWithSize WriteQuotedString(std::string_view value,
                                           char* buffer,
                                           size_t buffer_size) {
  constexpr size_t kOverhead = 2 /* quotes */ + 1 /* null */;
  if (value.size() + kOverhead > buffer_size) {
    return StatusWithSize::ResourceExhausted();
  }
  // If the string might fit, try to copy it. May still run out of room due to
  // escaping.
  const int written =
      EscapedStringCopy(buffer + 1 /* quote */, buffer_size - kOverhead, value);
  if (written < 0) {
    return StatusWithSize::ResourceExhausted();
  }

  buffer[0] = '"';            // open quote
  buffer[written + 1] = '"';  // close quote
  buffer[written + 2] = '\0';
  return StatusWithSize(written + 2);  // characters written excludes \0
}

constexpr StatusWithSize WriteCharPointer(const char* ptr,
                                          char* buffer,
                                          size_t buffer_size) {
  if (ptr == nullptr) {
    return WriteString("null", buffer, buffer_size);
  }
  return WriteQuotedString(ptr, buffer, buffer_size);
}

template <typename T>
inline constexpr bool kIsJson =
    std::is_base_of_v<JsonValue, T> || std::is_base_of_v<JsonArray, T> ||
    std::is_base_of_v<JsonObject, T>;

template <typename T>
struct InvalidJsonType : std::false_type {};

struct LiteralChars {
  const char (&open_close)[2];
};

template <typename T>
constexpr StatusWithSize SerializeJson(const T& value,
                                       char* buffer,
                                       size_t remaining) {
  if constexpr (kIsJson<T>) {  // nested JsonBuilder, JsonArray, JsonObject
    return WriteString(value, buffer, remaining);
  } else if constexpr (std::is_same_v<T, LiteralChars>) {  // Nested append/add
    return WriteString(
        std::string_view(value.open_close, 2), buffer, remaining);
  } else if constexpr (std::is_null_pointer_v<T> ||  // nullptr & C strings
                       std::is_same_v<T, char*> ||
                       std::is_same_v<T, const char*>) {
    return WriteCharPointer(value, buffer, remaining);
  } else if constexpr (std::is_convertible_v<T, std::string_view>) {  // strings
    return WriteQuotedString(value, buffer, remaining);
  } else if constexpr (std::is_floating_point_v<T>) {
    return string::FloatAsIntToString(value, {buffer, remaining});
  } else if constexpr (std::is_same_v<T, bool>) {  // boolean
    return WriteString(value ? "true" : "false", buffer, remaining);
  } else if constexpr (std::is_integral_v<T>) {  // integers
    return string::IntToString(value, {buffer, remaining});
  } else {
    static_assert(InvalidJsonType<T>(),
                  "JSON values may only be numbers, strings, JSON arrays, JSON "
                  "objects, or null");
    return StatusWithSize::Internal();
  }
}

}  // namespace json_impl

// Forward NestedJsonArray, NestedJsonObject, JsonArray, and JsonObject
// function calls to JsonBuilder.

#define PW_JSON_COMMON_INTERFACE_IMPL(name)                                    \
  constexpr bool name::IsValue() const {                                       \
    return static_cast<const JsonBuilder*>(this)->IsValue();                   \
  }                                                                            \
  constexpr bool name::IsArray() const {                                       \
    return static_cast<const JsonBuilder*>(this)->IsArray();                   \
  }                                                                            \
  constexpr bool name::IsObject() const {                                      \
    return static_cast<const JsonBuilder*>(this)->IsObject();                  \
  }                                                                            \
  constexpr name::operator std::string_view() const {                          \
    return static_cast<const JsonBuilder*>(this)->operator std::string_view(); \
  }                                                                            \
  constexpr const char* name::data() const {                                   \
    return static_cast<const JsonBuilder*>(this)->data();                      \
  }                                                                            \
  constexpr size_t name::size() const {                                        \
    return static_cast<const JsonBuilder*>(this)->size();                      \
  }                                                                            \
  constexpr size_t name::max_size() const {                                    \
    return static_cast<const JsonBuilder*>(this)->max_size();                  \
  }                                                                            \
  constexpr bool name::ok() const {                                            \
    return static_cast<const JsonBuilder*>(this)->ok();                        \
  }                                                                            \
  constexpr Status name::status() const {                                      \
    return static_cast<const JsonBuilder*>(this)->status();                    \
  }                                                                            \
  constexpr Status name::last_status() const {                                 \
    return static_cast<const JsonBuilder*>(this)->last_status();               \
  }                                                                            \
  constexpr void name::clear() {                                               \
    static_cast<JsonBuilder*>(this)->name##Clear();                            \
  }                                                                            \
  constexpr void name::clear_status() {                                        \
    static_cast<JsonBuilder*>(this)->clear_status();                           \
  }                                                                            \
  static_assert(true)

PW_JSON_COMMON_INTERFACE_IMPL(JsonValue);
PW_JSON_COMMON_INTERFACE_IMPL(JsonArray);
PW_JSON_COMMON_INTERFACE_IMPL(JsonObject);

#undef PW_JSON_COMMON_INTERFACE_IMPL

template <typename T>
constexpr NestedJsonArray& NestedJsonArray::Append(const T& value) {
  json_.builder().NestedJsonArrayAppend(value, json_.nesting());
  return *this;
}
constexpr NestedJsonArray NestedJsonArray::AppendNestedArray() {
  return json_.builder().JsonArrayAppendNested(json_impl::kArray,
                                               json_.nesting());
}
constexpr NestedJsonObject NestedJsonArray::AppendNestedObject() {
  return json_.builder().JsonArrayAppendNested(json_impl::kObject,
                                               json_.nesting());
}
constexpr NestedJsonArray NestedJsonObject::AddNestedArray(
    std::string_view key) {
  return json_.builder().JsonObjectAddNested(
      key, json_impl::kArray, json_.nesting());
}
constexpr NestedJsonObject NestedJsonObject::AddNestedObject(
    std::string_view key) {
  return json_.builder().JsonObjectAddNested(
      key, json_impl::kObject, json_.nesting());
}
template <typename T>
constexpr NestedJsonObject& NestedJsonObject::Add(std::string_view key,
                                                  const T& value) {
  json_.builder().NestedJsonObjectAdd(key, value, json_.nesting());
  return *this;
}
template <typename T>
constexpr Status JsonValue::Set(const T& value) {
  return static_cast<JsonBuilder*>(this)->JsonValueSet(value);
}
template <typename T>
constexpr JsonArray& JsonArray::Append(const T& value) {
  return static_cast<JsonBuilder*>(this)->JsonArrayAppend(value);
}
constexpr NestedJsonArray JsonArray::AppendNestedArray() {
  return static_cast<JsonBuilder*>(this)->JsonArrayAppendNested(
      json_impl::kArray, {});
}
constexpr NestedJsonObject JsonArray::AppendNestedObject() {
  return static_cast<JsonBuilder*>(this)->JsonArrayAppendNested(
      json_impl::kObject, {});
}
template <typename Iterable>
constexpr JsonArray& JsonArray::Extend(const Iterable& iterable) {
  return static_cast<JsonBuilder*>(this)->JsonArrayExtend(std::cbegin(iterable),
                                                          std::cend(iterable));
}
template <typename T, size_t kSize>
constexpr JsonArray& JsonArray::Extend(const T (&iterable)[kSize]) {
  return static_cast<JsonBuilder*>(this)->JsonArrayExtend(std::cbegin(iterable),
                                                          std::cend(iterable));
}
template <typename T>
constexpr JsonObject& JsonObject::Add(std::string_view key, const T& value) {
  return static_cast<JsonBuilder*>(this)->JsonObjectAdd(key, value);
}
constexpr NestedJsonArray JsonObject::AddNestedArray(std::string_view key) {
  return static_cast<JsonBuilder*>(this)->JsonObjectAddNested(
      key, json_impl::kArray, {});
}
constexpr NestedJsonObject JsonObject::AddNestedObject(std::string_view key) {
  return static_cast<JsonBuilder*>(this)->JsonObjectAddNested(
      key, json_impl::kObject, {});
}

// JsonBuilder function implementations.

template <typename T>
constexpr Status JsonBuilder::SetValue(const T& value) {
  return HandleSet(json_impl::SerializeJson(value, buffer_, max_size_ + 1));
}

constexpr void JsonBuilder::update_status(Status new_status) {
  last_status_ = new_status.code();
  if (!new_status.ok() && status().ok()) {
    status_ = new_status.code();
  }
}

template <typename T>
constexpr Status JsonBuilder::JsonValueSet(const T& value) {
  if constexpr (json_impl::kIsJson<T>) {
    PW_ASSERT(this != &value);   // Self-nesting is disallowed.
    PW_ASSERT(value.IsValue());  // Cannot set JsonValue to an array or object.
  }
  return SetValue(value);
}

template <typename T>
constexpr JsonArray& JsonBuilder::JsonArrayAppend(const T& value) {
  if constexpr (json_impl::kIsJson<T>) {
    PW_ASSERT(this != &value);  // Self-nesting is disallowed.
  }

  const size_t starting_size = size();
  if (JsonArrayAddElement()) {
    // The buffer size is remaining() + 1 bytes, but drop the + 1 to leave room
    // for the closing ].
    HandleAdd(json_impl::SerializeJson(value, &buffer_[size()], remaining()),
              starting_size,
              ']');
  }
  return *this;
}

template <typename Iterator>
constexpr JsonArray& JsonBuilder::JsonArrayExtend(Iterator begin,
                                                  Iterator end) {
  const size_t starting_size = size();

  for (Iterator cur = begin; cur != end; ++cur) {
    const Status status = Append(*cur).last_status();
    if (!status.ok()) {  // Undo changes if there is an issue.
      json_size_ = starting_size;
      buffer_[size() - 1] = ']';
      buffer_[size()] = '\0';
      break;
    }
  }
  return *this;
}

template <typename T>
constexpr JsonObject& JsonBuilder::JsonObjectAdd(std::string_view key,
                                                 const T& value) {
  if constexpr (json_impl::kIsJson<T>) {
    PW_ASSERT(this != &value);  // Self-nesting is disallowed.
  }

  const size_t starting_size = size();
  if (JsonObjectAddKey(key)) {
    // The buffer size is remaining() + 1 bytes, but drop the + 1 to leave room
    // for the closing }.
    HandleAdd(json_impl::SerializeJson(value, &buffer_[size()], remaining()),
              starting_size,
              '}');
  }
  return *this;
}

constexpr bool JsonBuilder::JsonArrayAddElement() {
  PW_ASSERT(IsArray());  // Attempted to append to an object or value

  // Needs space for at least 3 new characters (, 1)
  if (size() + 3 > max_size()) {
    update_status(Status::ResourceExhausted());
    return false;
  }

  // If this is the first element, just drop the ]. Otherwise, add a comma.
  if (size() == 2) {
    json_size_ = 1;
  } else {
    buffer_[json_size_ - 1] = ',';
    buffer_[json_size_++] = ' ';
  }

  return true;
}

constexpr bool JsonBuilder::JsonObjectAddKey(std::string_view key) {
  PW_ASSERT(IsObject());  // Attempted add a key-value pair to an array or value

  // Each key needs 7 more characters: (, "": ) plus at least 1 for the value.
  // The ',' replaces the terminal '}', but a new '}' is placed at the end, so
  // the total remains 7. The first key could get away with 5, but oh well.
  if (size() + key.size() + 7 > max_size()) {
    update_status(Status::ResourceExhausted());
    return false;
  }

  // If this is the first key, just drop the }. Otherwise, add a comma.
  if (size() == 2) {
    json_size_ = 1;
  } else {
    buffer_[json_size_ - 1] = ',';  // change the last } to ,
    buffer_[json_size_++] = ' ';
  }

  // Buffer size is remaining() + 1, but - 4 for at least ": 0}" after the key.
  auto written =
      json_impl::WriteQuotedString(key, &buffer_[json_size_], remaining() - 3);
  if (!written.ok()) {
    return false;
  }

  json_size_ += written.size();  // Now have {"key" or {..., "key"
  buffer_[json_size_++] = ':';
  buffer_[json_size_++] = ' ';
  return true;
}

constexpr json_impl::NestedJson JsonBuilder::JsonArrayAppendNested(
    const char (&open_close)[2], const json_impl::Nesting& nesting) {
  AddNestedStart(nesting);
  json_impl::Nesting::Type nesting_within = type();
  JsonArrayAppend(json_impl::LiteralChars{open_close});  // [..., {}]
  AddNestedFinish(nesting);
  return json_impl::NestedJson(
      *this,
      last_status().ok()
          ? nesting.Nest(NestedJsonOffset(nesting), nesting_within)
          : json_impl::Nesting());
}

constexpr json_impl::NestedJson JsonBuilder::JsonObjectAddNested(
    std::string_view key,
    const char (&open_close)[2],
    const json_impl::Nesting& nesting) {
  AddNestedStart(nesting);
  json_impl::Nesting::Type nesting_within = type();
  JsonObjectAdd(key, json_impl::LiteralChars{open_close});  // {..., "key": {}}
  AddNestedFinish(nesting);
  return json_impl::NestedJson(
      *this,
      last_status().ok()
          ? nesting.Nest(NestedJsonOffset(nesting), nesting_within)
          : json_impl::Nesting());
}

constexpr void JsonBuilder::AddNestedStart(const json_impl::Nesting& nesting) {
  // A nested structure must be the last thing in the JSON. Back up to where the
  // first of the closing ] or } should be, and check from there.
  PW_ASSERT(  // JSON must not have been cleared since nesting.
      json_size_ >= nesting.offset() + nesting.depth() + 2 /* [] or {} */);
  PW_ASSERT(  // Nested structure must match the expected type
      buffer_[json_size_ - nesting.depth() - 1] ==
      buffer_[nesting.offset()] + 2 /* convert [ to ] or { to } */);
  nesting.CheckNesting(&buffer_[json_size_ - nesting.depth()]);

  buffer_ += nesting.offset();
  json_size_ -= nesting.offset() + nesting.depth();
  max_size_ -= nesting.offset() + nesting.depth();
}

constexpr void JsonBuilder::AddNestedFinish(const json_impl::Nesting& nesting) {
  buffer_ -= nesting.offset();
  max_size_ += nesting.offset() + nesting.depth();

  json_size_ += nesting.offset();
  nesting.Terminate(&buffer_[json_size_]);
  json_size_ += nesting.depth();
}

template <typename T>
constexpr void JsonBuilder::NestedJsonArrayAppend(
    const T& value, const json_impl::Nesting& nesting) {
  AddNestedStart(nesting);
  JsonArrayAppend(value);
  AddNestedFinish(nesting);
}

template <typename T>
constexpr void JsonBuilder::NestedJsonObjectAdd(
    std::string_view key, const T& value, const json_impl::Nesting& nesting) {
  AddNestedStart(nesting);
  JsonObjectAdd(key, value);
  AddNestedFinish(nesting);
}

constexpr Status JsonBuilder::HandleSet(StatusWithSize written) {
  if (written.ok()) {
    json_size_ = written.size();
  } else {
    MakeNull();
  }
  set_statuses(written.status());  // status is always reset when setting value
  return last_status();
}

constexpr void JsonBuilder::HandleAdd(StatusWithSize written,
                                      size_t starting_size,
                                      char terminator) {
  update_status(written.status());  // save room for } or ]
  if (last_status().ok()) {
    json_size_ += written.size();
  } else {
    json_size_ = starting_size - 1;  // make room for closing character
  }

  buffer_[json_size_++] = terminator;
  buffer_[json_size_] = '\0';
}

}  // namespace pw
