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

use anyhow::{anyhow, Result};
use clap::Parser;
use system_generator::{Cli, DefaultSystemGenerator, SystemGenerator};

fn main() -> Result<()> {
    let cli = Cli::parse();
    let config = system_generator::parse_config(&cli)?;
    let mut system_generator = <DefaultSystemGenerator as SystemGenerator>::new(config);
    system_generator::generate(&cli, &mut system_generator).map_err(|e| anyhow!("{e}"))
}
