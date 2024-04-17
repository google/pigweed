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

/// @file pw_json/builder.h
///
/// The pw_json module provides utilities for interacting with <a
/// href="http://www.json.org">JSON</a>. JSON is a structured data format that
/// supports strings, integers, floating point numbers, booleans, null, arrays,
/// and objects (key-value pairs).
///
/// `pw::JsonBuilder` is a simple, efficient utility for serializing JSON to a
/// fixed-sized buffer. It works directly with the JSON wire format. It does not
/// support manipulation of previously serialized data.
///
/// All `JsonBuilder` functions are `constexpr`, so may be used in `constexpr`
/// and `constinit` statements.

#include <cstddef>
#include <string_view>
#include <type_traits>

#include "pw_assert/assert.h"
#include "pw_json/internal/nesting.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw {

/// @defgroup pw_json_builder_api
/// @{

/// A `JsonArray` nested inside an object or array. Only provides functions for
/// appending values to the nested array.
///
/// A `NestedJsonArray` is immediately invalidated if the enclosing JSON is
/// updated. Attempting to append to the nested array is an error.
class [[nodiscard]] NestedJsonArray {
 public:
  NestedJsonArray(const NestedJsonArray&) = delete;
  NestedJsonArray& operator=(const NestedJsonArray&) = delete;

  constexpr NestedJsonArray(NestedJsonArray&&) = default;
  constexpr NestedJsonArray& operator=(NestedJsonArray&&) = default;

  /// Appends to the nested array.
  template <typename T>
  constexpr NestedJsonArray& Append(const T& value);

  /// Appends a new nested array to this nested array.
  constexpr NestedJsonArray AppendNestedArray();

  /// Appends a new nested object to this nested array.
  constexpr NestedJsonObject AppendNestedObject();

 private:
  friend class JsonArray;
  friend class JsonObject;
  friend class NestedJsonObject;

  constexpr NestedJsonArray(json_impl::NestedJson&& nested)
      : json_(std::move(nested)) {}

  json_impl::NestedJson json_;
};

/// A `JsonObject` nested inside an array or object. Only provides functions for
/// adding key-value pairs to the nested object.
///
/// A `NestedJsonObject` is immediately invalidated if the enclosing JSON is
/// updated. Attempting to add to the nested object fails an assertion.
class [[nodiscard]] NestedJsonObject {
 public:
  NestedJsonObject(const NestedJsonObject&) = delete;
  NestedJsonObject& operator=(const NestedJsonObject&) = delete;

  constexpr NestedJsonObject(NestedJsonObject&&) = default;
  constexpr NestedJsonObject& operator=(NestedJsonObject&&) = default;

  /// Adds a key-value pair to the nested object.
  template <typename T>
  constexpr NestedJsonObject& Add(std::string_view key, const T& value);

  /// Adds a nested array to the nested object.
  constexpr NestedJsonArray AddNestedArray(std::string_view key);

  /// Adds a nested object to the nested object.
  constexpr NestedJsonObject AddNestedObject(std::string_view key);

 private:
  friend class JsonArray;
  friend class JsonObject;
  friend class NestedJsonArray;

  constexpr NestedJsonObject(json_impl::NestedJson&& nested)
      : json_(std::move(nested)) {}

  json_impl::NestedJson json_;
};

/// Stores a simple JSON value: a string, integer, float, boolean, or null.
/// Provides a `Set()` function as well as the common functions for accessing
/// the serialized data (see documentation for `JsonBuilder`).
class JsonValue {
 public:
  constexpr JsonValue(const JsonValue&) = delete;
  constexpr JsonValue& operator=(const JsonValue&) = delete;

  // Functions common to all JSON types.
  [[nodiscard]] constexpr bool IsValue() const;
  [[nodiscard]] constexpr bool IsArray() const;
  [[nodiscard]] constexpr bool IsObject() const;

  constexpr operator std::string_view() const;
  constexpr const char* data() const;
  constexpr size_t size() const;
  constexpr size_t max_size() const;

  [[nodiscard]] constexpr bool ok() const;
  constexpr Status status() const;
  constexpr Status last_status() const;
  constexpr void clear();
  constexpr void clear_status();

  /// Sets the JSON value to a boolean, number, string, or `null`. Sets and
  /// returns the status. If a `Set` call fails, the value is set to `null`.
  ///
  /// It is an error to call `Set()` on a `JsonValue` if `StartArray` or
  /// `StartObject` was called on the `JsonBuilder`. Setting the `JsonValue` to
  /// a JSON object or array is also an error.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The value serialized successfully.
  ///
  ///    RESOURCE_EXHAUSTED: There is insufficient buffer space to
  ///    serialize.
  ///
  /// @endrst
  template <typename T>
  constexpr Status Set(const T& value);

 private:
  friend class JsonBuilder;

  constexpr JsonValue() = default;
};

/// Stores a JSON array: a sequence of values. Provides functions for adding
/// items to the array, as well as the common functions for accessing the
/// serialized data (see documentation for `JsonBuilder`).
class JsonArray {
 public:
  constexpr JsonArray(const JsonArray&) = delete;
  constexpr JsonArray& operator=(const JsonArray&) = delete;

  // Functions common to all JSON types. See documentation for `JsonBuilder`.
  [[nodiscard]] constexpr bool IsValue() const;
  [[nodiscard]] constexpr bool IsArray() const;
  [[nodiscard]] constexpr bool IsObject() const;

  constexpr operator std::string_view() const;
  constexpr const char* data() const;
  constexpr size_t size() const;
  constexpr size_t max_size() const;

  [[nodiscard]] constexpr bool ok() const;
  constexpr Status status() const;
  constexpr Status last_status() const;
  constexpr void clear();
  constexpr void clear_status();

  /// Adds a value to the JSON array. Updates the status.
  ///
  /// It is an error to call `Append()` if the underlying `JsonBuilder` is no
  /// longer an array.
  template <typename T>
  constexpr JsonArray& Append(const T& value);

  /// Appends a nested array to this array.
  constexpr NestedJsonArray AppendNestedArray();

  /// Appends a nested object to this array.
  constexpr NestedJsonObject AppendNestedObject();

  /// Appends all elements from an iterable container. If there is an error,
  /// changes are reverted.
  template <typename Iterable>
  constexpr JsonArray& Extend(const Iterable& iterable);

  /// Appends all elements from an iterable container. If there is an error,
  /// changes are reverted.
  template <typename T, size_t kSize>
  constexpr JsonArray& Extend(const T (&iterable)[kSize]);

 private:
  friend class JsonBuilder;

  constexpr JsonArray() = default;
};

/// Stores a JSON object: a sequence of key-value pairs. Provides functions
/// for adding entries to the object, as well as the common functions for
/// accessing the serialized data (see documentation for `JsonBuilder`).
class JsonObject {
 public:
  constexpr JsonObject(const JsonObject&) = delete;
  constexpr JsonObject& operator=(const JsonObject&) = delete;

  // Functions common to all JSON types. See documentation for `JsonBuilder`.
  [[nodiscard]] constexpr bool IsValue() const;
  [[nodiscard]] constexpr bool IsArray() const;
  [[nodiscard]] constexpr bool IsObject() const;

  constexpr operator std::string_view() const;
  constexpr const char* data() const;
  constexpr size_t size() const;
  constexpr size_t max_size() const;

  [[nodiscard]] constexpr bool ok() const;
  constexpr Status status() const;
  constexpr Status last_status() const;
  constexpr void clear();
  constexpr void clear_status();

  /// Adds a key-value pair to the JSON object. Updates the status.
  ///
  /// It is an error to call `Add()` if the underlying `JsonBuilder` is no
  /// longer an object.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The value was appended successfully.
  ///
  ///    RESOURCE_EXHAUSTED: Insufficient buffer space to serialize.
  ///
  /// @endrst
  template <typename T>
  constexpr JsonObject& Add(std::string_view key, const T& value);

  template <typename T>
  constexpr JsonObject& Add(std::nullptr_t, const T& value) = delete;

  constexpr NestedJsonArray AddNestedArray(std::string_view key);

  constexpr NestedJsonObject AddNestedObject(std::string_view key);

 private:
  friend class JsonBuilder;

  constexpr JsonObject() = default;
};

/// `JsonBuilder` is used to create arbitrary JSON. Contains a JSON value, which
/// may be an object or array. Arrays and objects may contain other values,
/// objects, or arrays.
class JsonBuilder : private JsonValue, private JsonArray, private JsonObject {
 public:
  /// `JsonBuilder` requires at least 5 characters in its buffer.
  static constexpr size_t MinBufferSize() { return 5; }

  /// Initializes to the value `null`. `buffer.size()` must be at least 5.
  constexpr JsonBuilder(span<char> buffer)
      : JsonBuilder(buffer.data(), buffer.size()) {}

  /// Initializes to the value `null`. `buffer_size` must be at least 5.
  constexpr JsonBuilder(char* buffer, size_t buffer_size)
      : JsonBuilder(buffer, buffer_size, Uninitialized{}) {
    PW_ASSERT(buffer_size >= MinBufferSize());  // Must be at least 5 characters
    MakeNull();
  }

  /// True if the top-level JSON entity is a simple value (not array or object).
  [[nodiscard]] constexpr bool IsValue() const {
    return !IsObject() && !IsArray();
  }

  /// True if the top-level JSON entity is an array.
  [[nodiscard]] constexpr bool IsArray() const { return buffer_[0] == '['; }

  /// True if the top-level JSON entity is an object.
  [[nodiscard]] constexpr bool IsObject() const { return buffer_[0] == '{'; }

  /// `JsonBuilder` converts to `std::string_view`.
  constexpr operator std::string_view() const { return {data(), size()}; }

  /// Pointer to the serialized JSON, which is always a null-terminated string.
  constexpr const char* data() const { return buffer_; }

  /// The current size of the JSON string, excluding the null terminator.
  constexpr size_t size() const { return json_size_; }

  /// The maximum size of the JSON string, excluding the null terminator.
  constexpr size_t max_size() const { return max_size_; }

  /// True if @cpp_func{status} is @pw_status{OK}; no errors have occurred.
  [[nodiscard]] constexpr bool ok() const { return status().ok(); }

  /// Returns the `JsonBuilder`'s status, which reflects the first error that
  /// occurred while updating the JSON. After an update fails, the non-`OK`
  /// status remains until it is reset with `clear`, `clear_status`, or
  /// `SetValue`.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: All previous updates have succeeded.
  ///
  ///    RESOURCE_EXHAUSTED: An update did not fit in the buffer.
  ///
  /// @endrst
  constexpr Status status() const { return static_cast<Status::Code>(status_); }

  /// Returns the status from the most recent change to the JSON. This is set
  /// with each JSON update and may be @pw_status{OK} while `status()` is not.
  constexpr Status last_status() const {
    return static_cast<Status::Code>(last_status_);
  }

  /// Sets the JSON `null` and clears the status.
  constexpr void clear() { JsonValueClear(); }

  /// Resets `status()` and `last_status()`.
  constexpr void clear_status() { set_statuses(OkStatus()); }

  /// Clears the JSON and sets it to a single JSON value (see `JsonValue::Set`).
  template <typename T>
  constexpr Status SetValue(const T& value);

  /// Sets the JSON to `null` and returns a `JsonValue` reference to this
  /// `JsonBuilder`.
  [[nodiscard]] constexpr JsonValue& StartValue() {
    JsonValueClear();
    return *this;
  }

  /// Clears the JSON and sets it to an empty array (`[]`). Returns a
  /// `JsonArray` reference to this `JsonBuilder`. For example:
  ///
  /// @code{.cpp}
  ///   builder.StartArray()
  ///       .Append("item1")
  ///       .Append(2)
  ///       .Extend({"3", "4", "5"});
  /// @endcode
  [[nodiscard]] constexpr JsonArray& StartArray() {
    JsonArrayClear();
    return *this;
  }

  /// Clears the JSON and sets it to an empty object (`{}`). Returns a
  /// `JsonObject` reference to this `JsonBuilder`. For example:
  ///
  /// @code{.cpp}
  ///   JsonBuffer<128> builder;
  ///   JsonObject& object = builder.StartObject()
  ///       .Add("key1", 1)
  ///       .Add("key2", "val2");
  ///   object.Add("another", "entry");
  /// @endcode
  [[nodiscard]] constexpr JsonObject& StartObject() {
    JsonObjectClear();
    return *this;
  }

 protected:
  enum class Uninitialized {};

  // Constructor that doesn't initialize the buffer.
  constexpr JsonBuilder(char* buffer, size_t buffer_size, Uninitialized)
      : buffer_(buffer),
        max_size_(buffer_size - 1),
        json_size_(0),
        status_(OkStatus().code()),
        last_status_(OkStatus().code()) {}

  // Sets the buffer to the null value.
  constexpr void MakeNull() {
    buffer_[0] = 'n';
    buffer_[1] = 'u';
    buffer_[2] = 'l';
    buffer_[3] = 'l';
    buffer_[4] = '\0';
    json_size_ = 4;
  }

  constexpr void set_json_size(size_t json_size) { json_size_ = json_size; }

  constexpr void set_statuses(Status status, Status last_status) {
    status_ = status.code();
    last_status_ = last_status.code();
  }

 private:
  friend class JsonValue;
  friend class JsonArray;
  friend class JsonObject;

  friend class NestedJsonArray;
  friend class NestedJsonObject;

  constexpr size_t remaining() const { return max_size() - size(); }

  // Sets last_status_ and updates status_ if an error occurred.
  constexpr void update_status(Status new_status);

  constexpr void set_statuses(Status status) { set_statuses(status, status); }

  constexpr void JsonValueClear() {
    MakeNull();
    set_statuses(OkStatus());
  }

  constexpr void JsonArrayClear() {
    MakeEmpty('[', ']');
    set_statuses(OkStatus());
  }

  constexpr void JsonObjectClear() {
    MakeEmpty('{', '}');
    set_statuses(OkStatus());
  }

  template <typename T>
  constexpr Status JsonValueSet(const T& value);

  template <typename T>
  constexpr JsonArray& JsonArrayAppend(const T& value);

  template <typename Iterator>
  constexpr JsonArray& JsonArrayExtend(Iterator begin, Iterator end);

  template <typename T>
  constexpr JsonObject& JsonObjectAdd(std::string_view key, const T& value);

  [[nodiscard]] constexpr bool JsonArrayAddElement();

  // Adds the key if there's room for the key and at least one more character.
  [[nodiscard]] constexpr bool JsonObjectAddKey(std::string_view key);

  constexpr size_t NestedJsonOffset(const json_impl::Nesting& nesting) const {
    // Point to the start of the nested JSON array or object. This will be three
    // characters, plus one for each prior layer of nesting {..., "": []}.
    return json_size_ - 3 - nesting.depth();
  }

  constexpr json_impl::Nesting::Type type() const {
    return IsArray() ? json_impl::Nesting::kArray : json_impl::Nesting::kObject;
  }

  constexpr json_impl::NestedJson JsonArrayAppendNested(
      const char (&open_close)[2], const json_impl::Nesting& nesting);

  constexpr json_impl::NestedJson JsonObjectAddNested(
      std::string_view key,
      const char (&open_close)[2],
      const json_impl::Nesting& nesting);

  // Nesting works by shrinking the JsonBuilder to be just the nested structure,
  // then expanding back out when done adding items.
  constexpr void AddNestedStart(const json_impl::Nesting& nesting);
  constexpr void AddNestedFinish(const json_impl::Nesting& nesting);

  template <typename T>
  constexpr void NestedJsonArrayAppend(const T& value,
                                       const json_impl::Nesting& nesting);

  template <typename T>
  constexpr void NestedJsonObjectAdd(std::string_view key,
                                     const T& value,
                                     const json_impl::Nesting& nesting);

  // For a single JSON value, checks if writing succeeded and clears on failure.
  constexpr Status HandleSet(StatusWithSize written);

  // For a value added to an array or object, checks if writing the characters
  // succeeds, sets the status, and terminates the buffer as appropriate.
  constexpr void HandleAdd(StatusWithSize written,
                           size_t starting_size,
                           char terminator);

  constexpr void MakeEmpty(char open, char close) {
    buffer_[0] = open;
    buffer_[1] = close;
    buffer_[2] = '\0';
    json_size_ = 2;
  }

  // TODO: b/326097937 - Use StringBuilder here.
  char* buffer_;
  size_t max_size_;  // max size of the JSON string, excluding the '\0'
  size_t json_size_;

  // If any errors have occurred, status_ stores the most recent error Status.
  // last_status_ stores the status from the most recent operation.
  uint8_t status_;
  uint8_t last_status_;
};

/// A `JsonBuilder` with an integrated buffer. The buffer will be sized to fit
/// `kMaxSize` characters.
template <size_t kMaxSize>
class JsonBuffer final : public JsonBuilder {
 public:
  // Constructs a JsonBuffer with the value null.
  constexpr JsonBuffer()
      : JsonBuilder(static_buffer_, sizeof(static_buffer_), Uninitialized{}),
        static_buffer_{} {
    MakeNull();
  }

  template <typename T>
  static constexpr JsonBuffer Value(const T& initial_value) {
    JsonBuffer<kMaxSize> json;
    PW_ASSERT(json.SetValue(initial_value).ok());  // Failed serialization.
    return json;
  }

  // JsonBuffers may be copied or assigned, as long as the source buffer is not
  // larger than this buffer.
  constexpr JsonBuffer(const JsonBuffer& other) : JsonBuffer() {
    CopyFrom(other);
  }

  template <size_t kOtherSize>
  constexpr JsonBuffer(const JsonBuffer<kOtherSize>& other) : JsonBuffer() {
    CopyFrom(other);
  }

  constexpr JsonBuffer& operator=(const JsonBuffer& rhs) {
    CopyFrom(rhs);
    return *this;
  }

  template <size_t kOtherSize>
  constexpr JsonBuffer& operator=(const JsonBuffer<kOtherSize>& rhs) {
    CopyFrom(rhs);
    return *this;
  }

  static constexpr size_t max_size() { return kMaxSize; }

 private:
  static_assert(kMaxSize + 1 /* null */ >= JsonBuilder::MinBufferSize(),
                "JsonBuffers require at least 4 bytes");

  template <size_t kOtherSize>
  constexpr void CopyFrom(const JsonBuffer<kOtherSize>& other) {
    static_assert(kOtherSize <= kMaxSize,
                  "A JsonBuffer cannot be copied into a smaller buffer");
    CopyFrom(static_cast<const JsonBuilder&>(other));
  }

  constexpr void CopyFrom(const JsonBuilder& other) {
    for (size_t i = 0; i < other.size() + 1 /* include null */; ++i) {
      static_buffer_[i] = other.data()[i];
    }
    JsonBuilder::set_json_size(other.size());
    JsonBuilder::set_statuses(other.status(), other.last_status());
  }

  char static_buffer_[kMaxSize + 1];
};

/// @}

}  // namespace pw

// Functions are defined inline in a separate header.
#include "pw_json/internal/builder_impl.h"
