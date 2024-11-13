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

#include <cstddef>
#include <cstdint>

namespace pw::elf {

// ELF data types
using Elf32_Half = uint16_t;
using Elf64_Half = uint16_t;

using Elf32_Word = uint32_t;
using Elf64_Word = uint32_t;
using Elf32_Sword = int32_t;
using Elf64_Sword = int32_t;

using Elf32_Xword = uint64_t;
using Elf64_Xword = uint64_t;
using Elf32_Sxword = int64_t;
using Elf64_Sxword = int64_t;

using Elf32_Addr = uint32_t;
using Elf64_Addr = uint64_t;

using Elf32_Off = uint32_t;
using Elf64_Off = uint64_t;

// ELF constants
inline constexpr size_t EI_NIDENT = 16;

inline constexpr size_t EI_MAG0 = 0;
inline constexpr unsigned char ELFMAG0 = 0x7f;

inline constexpr size_t EI_MAG1 = 1;
inline constexpr unsigned char ELFMAG1 = 'E';

inline constexpr size_t EI_MAG2 = 2;
inline constexpr unsigned char ELFMAG2 = 'L';

inline constexpr size_t EI_MAG3 = 3;
inline constexpr unsigned char ELFMAG3 = 'F';

inline constexpr size_t EI_CLASS = 4;
inline constexpr unsigned char ELFCLASS32 = 1;
inline constexpr unsigned char ELFCLASS64 = 2;

inline constexpr size_t EI_DATA = 5;
inline constexpr unsigned char ELFDATA2LSB = 1;
inline constexpr unsigned char ELFDATA2MSB = 2;

// ELF structures
struct Elf32_Ehdr {
  unsigned char e_ident[EI_NIDENT];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};
static_assert(sizeof(Elf32_Ehdr) == 52);

struct Elf32_Shdr {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
};
static_assert(sizeof(Elf32_Shdr) == 40);

struct Elf64_Ehdr {
  unsigned char e_ident[EI_NIDENT];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
};
static_assert(sizeof(Elf64_Ehdr) == 64);

struct Elf64_Shdr {
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
};
static_assert(sizeof(Elf64_Shdr) == 64);

}  // namespace pw::elf
