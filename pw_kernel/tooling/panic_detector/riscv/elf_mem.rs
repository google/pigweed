// Copyright 2025 The Pigweed Authors
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

use core::ops::Bound;
use std::collections::BTreeMap;

use anyhow::anyhow;
use object::Endian;
use object::read::elf::{ElfFile32, ProgramHeader};
/// Gives access to the program data (.text, .rodata, etc) in an elf file, based
/// on the virtual memory address.
pub struct ElfMem<'a> {
    // Maps a virtual address to a slice of data to be loaded into that address
    // from the elf file.
    pub chunks: BTreeMap<u32, &'a [u8]>,
}
impl<'a> ElfMem<'a> {
    pub fn new<E: Endian>(elf: &'a ElfFile32<'a, E>, e: E) -> anyhow::Result<Self> {
        let mut chunks = BTreeMap::<u32, &[u8]>::new();
        // Create a chunk of "address space" for each program-header in the ELF file.
        for ph in elf.elf_program_headers() {
            // TODO: Correctly handle program headers that contain more
            // (zero-initialized) data than exists in the file.
            chunks.insert(
                ph.p_vaddr.get(e),
                ph.data(e, elf.data())
                    .map_err(|_| anyhow!("Unable to get program header data"))?,
            );
        }
        Ok(Self { chunks })
    }
    /// Return a slice of len `len` containing the program data at virtual
    /// address `addr`, or None if the elf file does not contain data for that
    /// address.
    #[must_use]
    pub fn get(&self, addr: u32, len: u32) -> Option<&'a [u8]> {
        // Find the chunk that starts before or at addr.
        let (chunk_addr, chunk_data) = self
            .chunks
            .range((Bound::Unbounded, Bound::Included(addr)))
            .next_back()?;
        if addr < *chunk_addr {
            return None;
        }
        let Ok(chunk_data_len) = u32::try_from(chunk_data.len()) else {
            return None;
        };
        let chunk_end_addr = chunk_addr.checked_add(chunk_data_len)?;
        let end_addr = addr.checked_add(len)?;
        if end_addr > chunk_end_addr {
            return None;
        }
        let Ok(addr) = usize::try_from(addr) else {
            return None;
        };
        let Ok(chunk_addr) = usize::try_from(*chunk_addr) else {
            return None;
        };
        let Ok(len) = usize::try_from(len) else {
            return None;
        };
        Some(&chunk_data[addr - chunk_addr..][..len])
    }
}
