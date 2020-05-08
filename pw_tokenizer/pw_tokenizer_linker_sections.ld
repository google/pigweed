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
 * tokenization. All sections have type INFO so they are excluded from the final
 * binary.
 *
 * The contents of this script can be copied into an existing linker script.
 * Alternately, this file can be directly included in a linker script with an
 * include directive. For example,
 *
 *   INCLUDE path/to/modules/pw_tokenizer/pw_tokenizer_linker_sections.ld
 *
 *   SECTIONS
 *   {
 *     (your existing linker sections)
 *   }
 */

SECTIONS
{
  /*
   * This section stores metadata that may be used during tokenized string
   * decoding. This metadata describes properties that may affect how the
   * tokenized string is encoded or decoded -- the maximum length of the hash
   * function and the sizes of certain integer types.
   *
   * Metadata is declared as key-value pairs. See the metadata variable in
   * tokenize.cc for further details.
   */
  .pw_tokenizer_info 0x0 (INFO) :
  {
    KEEP(*(.pw_tokenizer_info))
  }

  /*
   * Tokenized strings are stored in this section by default. In the compiled
   * code, format string literals are replaced by a hash of the string contents
   * and a compact argument list encoded in a uint32_t. The compiled code
   * contains no references to the tokenized strings in this section.
   *
   * The section contents are declared with KEEP so that they are not removed
   * from the ELF. These are never emitted in the final binary or loaded into
   * memory.
   */
  .pw_tokenized.default 0x0 (INFO) :
  {
    KEEP(*(.pw_tokenized.default.*))
  }

/*
 * Projects may define additional tokenization domains, if desired. Strings in
 * different domains are stored in separate ELF sections so they can be
 * processed separately by the token database tools.
 *
 * Use cases for domains include keeping large sets of strings separate to avoid
 * collisions, or separating a small subset of strings that will use truncated
 * tokens (e.g. 16-bit tokens instead of 32-bit tokens).
 *
 * Each tokenization domain in use must have a corresponding section in the
 * linker script. As required, copy this section declaration and replace
 * YOUR_CUSTOM_TOKENIZATION_DOMAIN with the the domain name.

  .pw_tokenized.YOUR_CUSTOM_TOKENIZATION_DOMAIN 0x0 (INFO) :
  {
    KEEP(*(.pw_tokenized.YOUR_CUSTOM_TOKENIZATION_DOMAIN.*))
  }

 */
}
