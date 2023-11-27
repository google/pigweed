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

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace pw::tokenizer {

/// Reads entries from a v0 binary token string database. This class does not
/// copy or modify the contents of the database.
///
/// The v0 token database has two significant shortcomings:
///
///   - Strings cannot contain null terminators (`\0`). If a string contains a
///     `\0`, the database will not work correctly.
///   - The domain is not included in entries. All tokens belong to a single
///     domain, which must be known independently.
///
/// A v0 binary token database is comprised of a 16-byte header followed by an
/// array of 8-byte entries and a table of null-terminated strings. The header
/// specifies the number of entries. Each entry contains information about a
/// tokenized string: the token and removal date, if any. All fields are little-
/// endian.
///
/// The token removal date is stored within an unsigned 32-bit integer. It is
/// stored as `<day> <month> <year>`, where `<day>` and `<month>` are 1 byte
/// each and `<year>` is two bytes. The fields are set to their maximum value
/// (`0xFF` or `0xFFFF`) if they are unset. With this format, dates may be
/// compared naturally as unsigned integers.
///
/// @rst
///   ======  ====  =========================
///   Header (16 bytes)
///   ---------------------------------------
///   Offset  Size  Field
///   ======  ====  =========================
///        0     6  Magic number (``TOKENS``)
///        6     2  Version (``00 00``)
///        8     4  Entry count
///       12     4  Reserved
///   ======  ====  =========================
///
///   ======  ====  ==================================
///   Entry (8 bytes)
///   ------------------------------------------------
///   Offset  Size  Field
///   ======  ====  ==================================
///        0     4  Token
///        4     1  Removal day (1-31, 255 if unset)
///        5     1  Removal month (1-12, 255 if unset)
///        6     2  Removal year (65535 if unset)
///   ======  ====  ==================================
/// @endrst
///
/// Entries are sorted by token. A string table with a null-terminated string
/// for each entry in order follows the entries.
///
/// Entries are accessed by iterating over the database. A O(n) `Find` function
/// is also provided. In typical use, a `TokenDatabase` is preprocessed by a
/// `pw::tokenizer::Detokenizer` into a `std::unordered_map`.
class TokenDatabase {
 private:
  // Internal struct that describes how the underlying binary token database
  // stores entries. RawEntries generally should not be used directly. Instead,
  // use an Entry, which contains a pointer to the entry's string.
  struct RawEntry {
    uint32_t token;
    uint32_t date_removed;
  };

  static_assert(sizeof(RawEntry) == 8u);

  template <typename T>
  static constexpr uint32_t ReadUint32(const T* bytes) {
    return static_cast<uint8_t>(bytes[0]) |
           static_cast<uint8_t>(bytes[1]) << 8 |
           static_cast<uint8_t>(bytes[2]) << 16 |
           static_cast<uint8_t>(bytes[3]) << 24;
  }

 public:
  /// An entry in the token database.
  struct Entry {
    /// The token that represents this string.
    uint32_t token;

    /// The date the token and string was removed from the database, or
    /// `0xFFFFFFFF` if it was never removed. Dates are encoded such that
    /// natural integer sorting sorts from oldest to newest dates. The day is
    /// stored an an 8-bit day, 8-bit month, and 16-bit year, packed into a
    /// little-endian `uint32_t`.
    uint32_t date_removed;

    /// The null-terminated string represented by this token.
    const char* string;
  };

  /// Iterator for `TokenDatabase` values.
  class iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = const Entry*;
    using reference = const Entry&;
    using iterator_category = std::forward_iterator_tag;

    constexpr iterator() : entry_{}, raw_(nullptr) {}

    constexpr iterator(const iterator& other) = default;
    constexpr iterator& operator=(const iterator& other) = default;

    constexpr iterator& operator++() {
      raw_ += sizeof(RawEntry);
      ReadRawEntry();
      // Move string_ to the character beyond the next null terminator.
      while (*entry_.string++ != '\0') {
      }
      return *this;
    }
    constexpr iterator operator++(int) {
      iterator previous(*this);
      operator++();
      return previous;
    }
    constexpr bool operator==(const iterator& rhs) const {
      return raw_ == rhs.raw_;
    }
    constexpr bool operator!=(const iterator& rhs) const {
      return raw_ != rhs.raw_;
    }

    constexpr const Entry& operator*() const { return entry_; }

    constexpr const Entry* operator->() const { return &entry_; }

    constexpr difference_type operator-(const iterator& rhs) const {
      return (raw_ - rhs.raw_) / sizeof(RawEntry);
    }

   private:
    friend class TokenDatabase;

    // Constructs a new iterator to a valid entry.
    constexpr iterator(const char* raw_entry, const char* string)
        : entry_{0, 0, string}, raw_{raw_entry} {
      if (raw_entry != string) {  // raw_entry == string if the DB is empty
        ReadRawEntry();
      }
    }

    explicit constexpr iterator(const char* end) : entry_{}, raw_(end) {}

    constexpr void ReadRawEntry() {
      entry_.token = ReadUint32(raw_);
      entry_.date_removed = ReadUint32(raw_ + sizeof(entry_.token));
    }

    Entry entry_;
    const char* raw_;
  };

  using value_type = Entry;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = const value_type*;
  using const_pointer = const value_type*;
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /// A list of token entries returned from a `Find` operation. This object can
  /// be iterated over or indexed as an array.
  class Entries {
   public:
    constexpr Entries(const iterator& begin, const iterator& end)
        : begin_(begin), end_(end) {}

    // The number of entries in this list.
    constexpr size_type size() const { return end_ - begin_; }

    // True of the list is empty.
    constexpr bool empty() const { return begin_ == end_; }

    // Accesses the specified entry in this set. The index must be less than
    // size(). This operation is O(n) in size().
    Entry operator[](size_type index) const;

    constexpr const iterator& begin() const { return begin_; }
    constexpr const iterator& end() const { return end_; }

   private:
    iterator begin_;
    iterator end_;
  };

  /// Returns true if the provided data is a valid token database. This checks
  /// the magic number (`TOKENS`), version (which must be `0`), and that there
  /// is is one string for each entry in the database. A database with extra
  /// strings or other trailing data is considered valid.
  template <typename ByteArray>
  static constexpr bool IsValid(const ByteArray& bytes) {
    return HasValidHeader(bytes) && EachEntryHasAString(bytes);
  }

  /// Creates a `TokenDatabase` and checks if the provided data is valid at
  /// compile time. Accepts references to constexpr containers (`array`, `span`,
  /// `string_view`, etc.) with static storage duration. For example:
  ///
  ///  @code{.cpp}
  ///
  ///    constexpr char kMyData[] = ...;
  ///    constexpr TokenDatabase db = TokenDatabase::Create<kMyData>();
  ///
  ///  @endcode
  template <const auto& kDatabaseBytes>
  static constexpr TokenDatabase Create() {
    static_assert(
        HasValidHeader<decltype(kDatabaseBytes)>(kDatabaseBytes),
        "Databases must start with a 16-byte header that begins with TOKENS.");

    static_assert(EachEntryHasAString<decltype(kDatabaseBytes)>(kDatabaseBytes),
                  "The database must have at least one string for each entry.");

    return TokenDatabase(std::data(kDatabaseBytes));
  }

  /// Creates a `TokenDatabase` from the provided byte array. The array may be a
  /// span, array, or other container type. If the data is not valid, returns a
  /// default-constructed database for which ok() is false.
  ///
  /// Prefer the `Create` overload that takes the data as a template parameter
  /// when possible, since that overload verifies data integrity at compile
  /// time.
  template <typename ByteArray>
  static constexpr TokenDatabase Create(const ByteArray& database_bytes) {
    return IsValid<ByteArray>(database_bytes)
               ? TokenDatabase(std::data(database_bytes))
               : TokenDatabase();  // Invalid database.
  }
  /// Creates a database with no data. `ok()` returns false.
  constexpr TokenDatabase() : begin_{.data = nullptr}, end_{.data = nullptr} {}

  /// Returns all entries associated with this token. This is `O(n)`.
  Entries Find(uint32_t token) const;

  /// Returns the total number of entries (unique token-string pairs).
  constexpr size_type size() const {
    return (end_.data - begin_.data) / sizeof(RawEntry);
  }

  /// True if this database was constructed with valid data. The database might
  /// be empty, but it has an intact header and a string for each entry.
  constexpr bool ok() const { return begin_.data != nullptr; }

  /// Returns an iterator for the first token entry.
  constexpr iterator begin() const { return iterator(begin_.data, end_.data); }

  /// Returns an iterator for one past the last token entry.
  constexpr iterator end() const { return iterator(end_.data); }

 private:
  struct Header {
    std::array<char, 6> magic;
    uint16_t version;
    uint32_t entry_count;
    uint32_t reserved;
  };

  static_assert(sizeof(Header) == 2 * sizeof(RawEntry));

  template <typename ByteArray>
  static constexpr bool HasValidHeader(const ByteArray& bytes) {
    static_assert(sizeof(*std::data(bytes)) == 1u);

    if (std::size(bytes) < sizeof(Header)) {
      return false;
    }

    // Check the magic number and version.
    for (size_type i = 0; i < kMagicAndVersion.size(); ++i) {
      if (bytes[i] != kMagicAndVersion[i]) {
        return false;
      }
    }

    return true;
  }

  template <typename ByteArray>
  static constexpr bool EachEntryHasAString(const ByteArray& bytes) {
    const size_type entries = ReadEntryCount(std::data(bytes));

    // Check that the data is large enough to have a string table.
    if (std::size(bytes) < StringTable(entries)) {
      return false;
    }

    // Count the strings in the string table.
    size_type string_count = 0;
    for (auto i = std::begin(bytes) + StringTable(entries); i < std::end(bytes);
         ++i) {
      string_count += (*i == '\0') ? 1 : 0;
    }

    // Check that there is at least one string for each entry.
    return string_count >= entries;
  }

  // Reads the number of entries from a database header. Cast to the bytes to
  // uint8_t to avoid sign extension if T is signed.
  template <typename T>
  static constexpr uint32_t ReadEntryCount(const T* header_bytes) {
    const T* bytes = header_bytes + offsetof(Header, entry_count);
    return ReadUint32(bytes);
  }

  // Calculates the offset of the string table.
  static constexpr size_type StringTable(size_type entries) {
    return sizeof(Header) + entries * sizeof(RawEntry);
  }

  // The magic number that starts the table is "TOKENS". The version is encoded
  // next as two bytes.
  static constexpr std::array<char, 8> kMagicAndVersion = {
      'T', 'O', 'K', 'E', 'N', 'S', '\0', '\0'};

  template <typename Byte>
  constexpr TokenDatabase(const Byte bytes[])
      : TokenDatabase(bytes + sizeof(Header),
                      bytes + StringTable(ReadEntryCount(bytes))) {
    static_assert(sizeof(Byte) == 1u);
  }

  // It is illegal to reinterpret_cast in constexpr functions, but acceptable to
  // use unions. Instead of using a reinterpret_cast to change the byte pointer
  // to a RawEntry pointer, have a separate overload for each byte pointer type
  // and store them in a union.
  constexpr TokenDatabase(const char* begin, const char* end)
      : begin_{.data = begin}, end_{.data = end} {}

  constexpr TokenDatabase(const unsigned char* begin, const unsigned char* end)
      : begin_{.unsigned_data = begin}, end_{.unsigned_data = end} {}

  constexpr TokenDatabase(const signed char* begin, const signed char* end)
      : begin_{.signed_data = begin}, end_{.signed_data = end} {}

  // Store the beginning and end pointers as a union to avoid breaking constexpr
  // rules for reinterpret_cast.
  union {
    const char* data;
    const unsigned char* unsigned_data;
    const signed char* signed_data;
  } begin_, end_;
};

}  // namespace pw::tokenizer
