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

#include "pw_elf/reader.h"

#include <vector>

#include "pw_status/try.h"
#include "pw_stream/std_file_stream.h"

pw::Status ReaderExample() {
  pw::stream::StdFileReader stream("/tmp/example.elf");
  PW_TRY_ASSIGN(auto reader, pw::elf::ElfReader::FromStream(stream));

  pw::StatusWithSize section_size = reader.SeekToSection(".example");
  if (!section_size.ok()) {
    return pw::Status::NotFound();
  }

  std::vector<std::byte> section_data;
  section_data.resize(section_size.size());
  PW_TRY(stream.ReadExact(section_data));

  return pw::OkStatus();
}
