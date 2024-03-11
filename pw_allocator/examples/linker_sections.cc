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

#include <array>
#include <cstdint>
#include <string_view>

#include "examples/named_u32.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/block_allocator.h"
#include "pw_unit_test/framework.h"

namespace examples {

// The "real" `PW_PLACE_IN_SECTION` can be found in pw_preprocessor/compiler.h.
// For the purposes of keeping this example simple and free of linker-scripts,
// the macro is replaced with a no-op version.
#ifdef PW_PLACE_IN_SECTION
#undef PW_PLACE_IN_SECTION
#endif
#define PW_PLACE_IN_SECTION(section)

// DOCSTAG: [pw_allocator-examples-linker_sections-injection]
class NamedU32Factory {
 public:
  using Allocator = pw::allocator::Allocator;

  explicit NamedU32Factory(Allocator& allocator) : allocator_(allocator) {}

  auto MakeNamedU32(std::string_view name, uint32_t value) {
    return allocator_.MakeUnique<NamedU32>(name, value);
  }

 private:
  Allocator& allocator_;
};
// DOCSTAG: [pw_allocator-examples-linker_sections-injection]

// DOCSTAG: [pw_allocator-examples-linker_sections-placement]
// Set up an object that allocates from SRAM memory.
PW_PLACE_IN_SECTION(".sram") std::array<std::byte, 0x1000> sram_buffer;
pw::allocator::FirstFitBlockAllocator<uint16_t> sram_allocator(sram_buffer);
NamedU32Factory sram_factory(sram_allocator);

// Set up an object that allocates from PSRAM memory.
PW_PLACE_IN_SECTION(".psram") std::array<std::byte, 0x2000> psram_buffer;
pw::allocator::WorstFitBlockAllocator<uint32_t> psram_allocator(psram_buffer);
NamedU32Factory psram_factory(psram_allocator);
// DOCSTAG: [pw_allocator-examples-linker_sections-placement]

}  // namespace examples

namespace pw::allocator {

TEST(LinkerSectionExample, MakeObjects) {
  auto result1 = examples::sram_factory.MakeNamedU32("1", 1);
  EXPECT_TRUE(result1.has_value());

  auto result2 = examples::psram_factory.MakeNamedU32("2", 2);
  EXPECT_TRUE(result2.has_value());
}

}  // namespace pw::allocator
