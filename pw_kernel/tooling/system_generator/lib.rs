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

use core::str::FromStr;
use std::fs;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;

use anyhow::{Context, Error, Result, anyhow};
use clap::{Args, Parser, Subcommand};
use minijinja::{Environment, State};

pub mod system_config;

#[derive(Debug, Parser)]
pub struct Cli {
    #[command(flatten)]
    common_args: CommonArgs,
    #[command(subcommand)]
    command: Command,
}

#[derive(Args, Debug)]
pub struct CommonArgs {
    #[arg(long, required(true))]
    config: PathBuf,
    #[arg(long, required(true))]
    output: PathBuf,
    #[arg(
        long("template"),
        value_name = "NAME=PATH",
        value_parser = parse_template,
        action = clap::ArgAction::Append
    )]
    templates: Vec<(String, PathBuf)>,
}

fn parse_template(s: &str) -> Result<(String, PathBuf), String> {
    s.split_once('=')
        .map(|(name, path)| (name.to_string(), path.into()))
        .ok_or_else(|| format!("invalid template format: '{s}'. Expected NAME=PATH."))
}

#[derive(Subcommand, Debug)]
enum Command {
    CodegenSystem,
    AppLinkerScript(AppLinkerScriptArgs),
}

#[derive(Args, Debug)]
pub struct AppLinkerScriptArgs {
    #[arg(long, required = true)]
    pub app_name: String,
}

pub trait SystemGenerator {
    fn new(config: system_config::SystemConfig) -> Self;
    fn config(&self) -> &system_config::SystemConfig;
    fn populate_addresses(&mut self);

    fn render_system(&mut self, env: &mut Environment) -> Result<String> {
        let template = env.get_template("system")?;
        match template.render(self.config()) {
            Ok(str) => Ok(str),
            Err(e) => Err(anyhow!(e)),
        }
    }

    fn render_app_linker_script(&mut self, env: &Environment, app_name: &String) -> Result<String> {
        let template = env.get_template("app")?;
        match template.render(self.config().apps.get(app_name)) {
            Ok(str) => Ok(str),
            Err(e) => Err(anyhow!(e)),
        }
    }

    #[must_use]
    fn align(value: usize, alignment: usize) -> usize {
        debug_assert!(alignment.is_power_of_two());
        (value + alignment - 1) & !(alignment - 1)
    }
}

pub fn parse_config(cli: &Cli) -> Result<system_config::SystemConfig> {
    let json5_str =
        fs::read_to_string(&cli.common_args.config).context("Failed to read config file")?;
    serde_json5::from_str(&json5_str).context("Failed to parse config file")
}

pub fn generate<T: SystemGenerator>(cli: &Cli, system_generator: &mut T) -> Result<()> {
    let mut env = Environment::new();
    env.add_filter("to_hex", to_hex);

    for (name, path) in &cli.common_args.templates {
        let template = fs::read_to_string(path)?;
        env.add_template_owned(name, template)?;
    }

    system_generator.populate_addresses();

    let out_str = match &cli.command {
        Command::CodegenSystem => system_generator.render_system(&mut env)?,
        Command::AppLinkerScript(args) => {
            system_generator.render_app_linker_script(&env, &args.app_name)?
        }
    };
    // println!("OUT:\n{}", out_str);

    let mut file = File::create(&cli.common_args.output)?;
    file.write_all(out_str.as_bytes())
        .context("Failed to write output")
}

// The DefaultSystemGenerator below supports Armv8m and RiscV.
// New architectures which are implemented out of tree can
// depend on this crate with a custom SystemGenerator impl.

const FLASH_ALIGNMENT: usize = 4;
const RAM_ALIGNMENT: usize = 8;

#[derive(Debug, PartialEq)]
pub enum Arch {
    Armv8M,
    RiscV,
}

impl FromStr for Arch {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "armv8m" => Ok(Arch::Armv8M),
            "riscv" => Ok(Arch::RiscV),
            _ => Err(anyhow!("'{}' is not a valid architecture", s)),
        }
    }
}

pub struct DefaultSystemGenerator {
    config: system_config::SystemConfig,
}

impl SystemGenerator for DefaultSystemGenerator {
    fn new(config: system_config::SystemConfig) -> Self {
        DefaultSystemGenerator { config }
    }

    fn config(&self) -> &system_config::SystemConfig {
        &self.config
    }

    fn populate_addresses(&mut self) {
        // Stack the apps after the kernel in flash and ram.
        // TODO: davidroth - remove the requirement of setting the size of
        // flash, and instead calculate it based on code size.
        let mut next_flash_start_address =
            self.config.kernel.flash_start_address + self.config.kernel.flash_size_bytes;
        next_flash_start_address = Self::align(next_flash_start_address, FLASH_ALIGNMENT);
        let mut next_ram_start_address =
            self.config.kernel.ram_start_address + self.config.kernel.ram_size_bytes;
        next_ram_start_address = Self::align(next_ram_start_address, RAM_ALIGNMENT);

        let arch = self.config.arch.parse().unwrap();
        self.config.arch_crate_name = match arch {
            Arch::Armv8M => "arch_arm_cortex_m",
            Arch::RiscV => "arch_riscv",
        };

        for app in self.config.apps.values_mut() {
            app.flash_start_address = next_flash_start_address;
            next_flash_start_address = Self::align(
                app.flash_start_address + app.flash_size_bytes,
                FLASH_ALIGNMENT,
            );

            app.ram_start_address = next_ram_start_address;
            next_ram_start_address =
                Self::align(app.ram_start_address + app.ram_size_bytes, RAM_ALIGNMENT);

            app.start_fn_address = match arch {
                // On Armv8M, the +1 is to denote thumb mode.
                Arch::Armv8M => app.flash_start_address + 1,
                Arch::RiscV => app.flash_start_address,
            };

            app.initial_sp = app.ram_start_address + app.ram_size_bytes;
        }
    }
}

// Custom filters
#[must_use]
pub fn to_hex(_state: &State, value: usize) -> String {
    format!("{value:#x}")
}
