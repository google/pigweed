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

#include "pw_containers/intrusive_multimap.h"

#include "pw_unit_test/framework.h"

namespace examples {

// DOCSTAG: [pw_containers-intrusive_multimap]

struct Book : public pw::IntrusiveMultiMap<uint32_t, Book>::Item {
  const char* name;
  uint32_t oclc;

  Book(const char* name_, uint32_t oclc_) : name(name_), oclc(oclc_) {}

  // Indicates the key used to look up this item in the map.
  constexpr const uint32_t& key() const { return oclc; }
};

std::array<Book, 12> books = {{
    {"The Little Prince", 182537909u},
    {"Harry Potter and the Philosopher's Stone", 44795766u},
    {"Harry Potter and the Philosopher's Stone", 44795766u},
    {"Harry Potter and the Philosopher's Stone", 44795766u},
    {"Harry Potter and the Philosopher's Stone", 44795766u},
    {"Harry Potter and the Philosopher's Stone", 44795766u},
    {"The Hobbit", 1827184u},
    {"The Hobbit", 1827184u},
    {"The Hobbit", 1827184u},
    {"The Hobbit", 1827184u},
    {"Alice's Adventures in Wonderland", 5635965u},
    {"Alice's Adventures in Wonderland", 5635965u},
}};
pw::IntrusiveMultiMap<uint32_t, Book> library(books.begin(), books.end());

void VisitLibrary(pw::IntrusiveMultiMap<uint32_t, Book>& book_bag) {
  // Pick out some new books to read to the kids, but only if they're available.
  std::array<uint32_t, 3> oclcs = {
      1827184u,    // The Hobbit
      5635965u,    // Alice's Adventures in Wonderland
      182537909u,  // The Little Prince
  };
  for (uint32_t oclc : oclcs) {
    auto iter = library.find(oclc);
    if (iter != library.end()) {
      Book& book = *iter;
      library.erase(iter);
      book_bag.insert(book);
    }
  }
}

// DOCSTAG: [pw_containers-intrusive_multimap]

}  // namespace examples

namespace {

TEST(IntrusiveMultiMapExampleTest, TBD) {
  pw::IntrusiveMultiMap<uint32_t, examples::Book> book_bag1;
  examples::VisitLibrary(book_bag1);

  pw::IntrusiveMultiMap<uint32_t, examples::Book> book_bag2;
  examples::VisitLibrary(book_bag2);

  pw::IntrusiveMultiMap<uint32_t, examples::Book> book_bag3;
  examples::VisitLibrary(book_bag3);

  auto iter = book_bag1.begin();
  ASSERT_NE(iter, book_bag1.end());
  EXPECT_STREQ((iter++)->name, "The Hobbit");
  ASSERT_NE(iter, book_bag1.end());
  EXPECT_STREQ((iter++)->name, "Alice's Adventures in Wonderland");
  ASSERT_NE(iter, book_bag1.end());
  EXPECT_STREQ((iter++)->name, "The Little Prince");
  EXPECT_EQ(iter, book_bag1.end());
  book_bag1.clear();

  iter = book_bag2.begin();
  ASSERT_NE(iter, book_bag2.end());
  EXPECT_STREQ((iter++)->name, "The Hobbit");
  ASSERT_NE(iter, book_bag2.end());
  EXPECT_STREQ((iter++)->name, "Alice's Adventures in Wonderland");
  EXPECT_EQ(iter, book_bag2.end());
  book_bag2.clear();

  iter = book_bag3.begin();
  ASSERT_NE(iter, book_bag3.end());
  EXPECT_STREQ((iter++)->name, "The Hobbit");
  EXPECT_EQ(iter, book_bag3.end());
  book_bag3.clear();

  examples::library.clear();
}

}  // namespace
