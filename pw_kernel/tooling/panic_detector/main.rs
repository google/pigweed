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

#![allow(clippy::print_stdout)]

use std::path::PathBuf;

use anyhow::anyhow;
use clap::Parser;
use object::LittleEndian;

mod check_panics;
mod riscv;

#[derive(Debug, Parser)]
struct Args {
    #[arg(long, required(true))]
    image: PathBuf,
}

fn main() {
    let args = Args::parse();
    match crate::check_panics::check_panic(&args.image) {
        Ok(()) => std::process::exit(0),
        Err(..) => std::process::exit(1),
    }
}

fn find_symbol_address(
    t: &object::read::elf::SymbolTable<object::elf::FileHeader32<LittleEndian>>,
    name: &str,
) -> anyhow::Result<u64> {
    const E: object::LittleEndian = object::LittleEndian;
    for sym in t.symbols() {
        if t.symbol_name(E, sym)? == name.as_bytes() {
            return Ok(sym.st_value.get(E).into());
        }
    }
    Err(anyhow!("Unable to find symbol {name:?}"))
}
