/*
 * Copyright 2023 The Pigweed Authors
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
 */

/*
 * This file is separate from pw_tokenizer_linker_sections.ld because Zephyr
 * already defines the top level SECTIONS label and requires new linker
 * scripts to only add the individual sections. Additionally, the 0x0 address
 * is incompatible with Zephyr's linker scripts.
 *
 * See detailed section info at pw_tokenizer_linker_sections.ld
 */

#ifdef CONFIG_ARCH_POSIX
#define ADDR
#else
#define ADDR 0x0
#endif

.pw_tokenizer.info ADDR (INFO) :
{
  KEEP(*(.pw_tokenizer.info))
}

.pw_tokenizer.entries ADDR (INFO) :
{
  KEEP(*(.pw_tokenizer.entries.*))
}
