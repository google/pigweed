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

#include "pw_assert/assert.h"
#include "pw_elf/internal/elf.h"
#include "pw_elf/internal/stream_utils.h"
#include "pw_status/try.h"
#include "pw_stream/stream.h"

namespace pw::elf::internal {

template <class Elf_Ehdr, class Elf_Shdr>
class ElfReaderImpl {
 public:
  static Result<ElfReaderImpl> FromStream(stream::SeekableReader& stream) {
    ElfReaderImpl reader(stream);
    PW_TRY(reader.Initialize());
    return reader;
  }

  stream::SeekableReader& stream() const { return stream_; }

  StatusWithSize SeekToSection(std::string_view name) {
    for (unsigned int i = 0; i < sec_hdr_count(); ++i) {
      // Read the section header
      PW_TRY_WITH_SIZE(SeekToSectionHeader(i));
      auto section_hdr_ret = ReadObject<Elf_Shdr>(stream_);
      PW_TRY_WITH_SIZE(section_hdr_ret.status());
      Elf_Shdr section_hdr = std::move(section_hdr_ret.value());

      // Read the section name string
      PW_TRY_WITH_SIZE(stream_.Seek(
          str_table_data_off() + static_cast<ptrdiff_t>(section_hdr.sh_name)));
      auto section_name = ReadNullTermString(stream_);
      PW_TRY_WITH_SIZE(section_name.status());

      if (section_name.value() == name) {
        PW_TRY_WITH_SIZE(
            stream_.Seek(static_cast<ptrdiff_t>(section_hdr.sh_offset)));
        return StatusWithSize(section_hdr.sh_size);
      }
    }

    return StatusWithSize::NotFound();
  }

 private:
  ElfReaderImpl(stream::SeekableReader& stream) : stream_(stream) {}

  Status Initialize() {
    PW_TRY(stream_.Seek(0));

    // Read ELF file header
    PW_TRY_ASSIGN(file_header_, ReadObject<Elf_Ehdr>(stream_));

    // Note: e_ident already validated

    // Validate section header size
    if (file_header_->e_shentsize < sizeof(Elf_Shdr)) {
      return Status::DataLoss();
    }

    // Read string table section header
    PW_TRY(SeekToSectionHeader(file_header_->e_shstrndx));
    PW_TRY_ASSIGN(str_table_sec_hdr_, ReadObject<Elf_Shdr>(stream_));

    return OkStatus();
  }

  Status SeekToSectionHeader(unsigned int index) {
    PW_ASSERT(file_header_);
    return stream_.Seek(static_cast<ptrdiff_t>(
        file_header_->e_shoff + (index * file_header_->e_shentsize)));
  }

  ptrdiff_t str_table_data_off() const {
    PW_ASSERT(str_table_sec_hdr_);
    return static_cast<ptrdiff_t>(str_table_sec_hdr_->sh_offset);
  }

  unsigned int sec_hdr_count() const {
    PW_ASSERT(file_header_);
    return file_header_->e_shnum;
  }

  stream::SeekableReader& stream_;
  std::optional<Elf_Ehdr> file_header_;
  std::optional<Elf_Shdr> str_table_sec_hdr_;
};

using ElfReaderImpl32 = ElfReaderImpl<Elf32_Ehdr, Elf32_Shdr>;

#if UINTPTR_MAX == UINT64_MAX
using ElfReaderImpl64 = ElfReaderImpl<Elf64_Ehdr, Elf64_Shdr>;
#endif

using ElfReaderImpls = std::variant<ElfReaderImpl32
#if UINTPTR_MAX == UINT64_MAX
                                    ,
                                    ElfReaderImpl64
#endif
                                    >;
}  // namespace pw::elf::internal
