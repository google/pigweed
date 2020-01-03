/*
 * Copyright 2020 The Pigweed Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 *
 * This linker script snippet declares the sections needed for string
 * tokenization.
 *
 * This file may be directly included in a linker script with an include
 * directive. For example,
 *
 *   INCLUDE path/to/modules/pw_tokenizer/tokenizer_linker_sections.ld
 *
 *   SECTIONS
 *   {
 *     (your existing linker sections)
 *   }
 */

SECTIONS
{
  /*
   * All tokenized strings are stored in this section. Since the section has
   * type INFO, it is excluded from the final binary.
   *
   * In the compiled code, format string literals are replaced by a hash of the
   * string contents and a compact argument list encoded in a uint32_t. The
   * compiled code contains no references to the tokenized strings in this
   * section.
   *
   * The section contents are declared with KEEP so that they are not removed
   * from the ELF. These are never emitted in the final binary or loaded into
   * memory.
   */
  .tokenized 0x00000000 (INFO) :
  {
    KEEP(*(.tokenized))
    KEEP(*(.tokenized.*))
  }

  /*
   * This section stores metadata that may be used during tokenized string
   * decoding. This metadata describes properties that may affect how the
   * tokenized string is encoded or decoded -- the maximum length of the hash
   * function and the sizes of certain integer types.
   *
   * Metadata is declared as key-value pairs. See the metadata variable in
   * tokenize.cc for further details.
   */
  .tokenizer_info 0x00000000 (INFO) :
  {
    KEEP(*(.tokenizer_info))
  }
}
