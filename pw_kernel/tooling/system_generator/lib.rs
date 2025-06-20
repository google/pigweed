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

use anyhow::{anyhow, Error, Result};
use askama::Template;
use clap::{Args, Parser, Subcommand};
use serde::Deserialize;

use crate::system_config::filters;
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
}

#[derive(Subcommand, Debug)]
enum Command {
    #[command()]
    CodegenSystem,
    #[command()]
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
    fn render_arch_app_linker_script(
        &mut self,
        _app_config: &system_config::AppConfig,
    ) -> Result<String>;

    fn render_system(&mut self) -> Result<String> {
        self.populate_addresses();
        self.config()
            .render()
            .map_err(|e| anyhow!("Failed to render system: {e}"))
    }

    fn render_app_linker_script(&mut self, app_name: &String) -> Result<String> {
        self.populate_addresses();
        let app_config = self
            .config()
            .apps
            .get(app_name)
            .expect("'{app_name}' does not exist in the config file.")
            .clone();

        self.render_arch_app_linker_script(&app_config)
    }

    #[must_use]
    fn align(value: usize, alignment: usize) -> usize {
        debug_assert!(alignment.is_power_of_two());
        (value + alignment - 1) & !(alignment - 1)
    }
}

pub fn parse_config(cli: &Cli) -> Result<system_config::SystemConfig> {
    let toml_str = fs::read_to_string(&cli.common_args.config).expect("config file exists");
    Ok(toml::from_str(&toml_str).expect("config parses"))
}

pub fn generate<T: SystemGenerator>(cli: &Cli, system_generator: &mut T) -> Result<()> {
    let out_str = match &cli.command {
        Command::CodegenSystem => system_generator.render_system()?,
        Command::AppLinkerScript(args) => {
            system_generator.render_app_linker_script(&args.app_name)?
        }
    };
    // println!("OUT:\n{}", out_str);

    let mut file = File::create(&cli.common_args.output)?;
    file.write_all(out_str.as_bytes())
        .map_err(|e| anyhow!("Failed to write output: {e}"))
}

// The DefaultSystemGenerator below supports Armv7m and RiscV.
// New architectures which are implemented out of tree can
// depend on this crate with a custom SystemGenerator impl.

const FLASH_ALIGNMENT: usize = 4;
const RAM_ALIGNMENT: usize = 8;

#[derive(Template)]
#[template(path = "armv8m_app.ld.tmpl", escape = "none")]
#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AppConfigArmv8M {
    pub config: system_config::AppConfig,
}

#[derive(Template)]
#[template(path = "riscv_app.ld.tmpl", escape = "none")]
#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AppConfigRiscV {
    pub config: system_config::AppConfig,
}

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
        let mut flash_offset =
            self.config.kernel.flash_start_address + self.config.kernel.flash_size_bytes;
        flash_offset = Self::align(flash_offset, FLASH_ALIGNMENT);
        let mut ram_offset =
            self.config.kernel.ram_start_address + self.config.kernel.ram_size_bytes;
        ram_offset = Self::align(ram_offset, RAM_ALIGNMENT);

        let arch = self.config.arch.parse().unwrap();
        self.config.arch_crate_name = match arch {
            Arch::Armv8M => "arch_arm_cortex_m",
            Arch::RiscV => "arch_riscv",
        };

        for app in self.config.apps.values_mut() {
            app.flash_start_address = flash_offset;
            app.flash_end_address = app.flash_start_address + app.flash_size_bytes;
            flash_offset = Self::align(app.flash_end_address + 1, FLASH_ALIGNMENT);

            app.ram_start_address = ram_offset;
            app.ram_end_address = app.ram_start_address + app.ram_size_bytes;
            ram_offset = Self::align(app.ram_end_address + 1, RAM_ALIGNMENT);

            app.start_fn_address = match arch {
                // On Armv8M, the +1 is to denote thumb mode.
                Arch::Armv8M => app.flash_start_address + 1,
                Arch::RiscV => app.flash_start_address,
            };

            app.initial_sp = app.ram_end_address;
        }
    }

    fn render_arch_app_linker_script(
        &mut self,
        app_config: &system_config::AppConfig,
    ) -> Result<String> {
        let arch = self.config.arch.parse().unwrap();
        match arch {
            Arch::Armv8M => AppConfigArmv8M {
                config: app_config.clone(),
            }
            .render(),
            Arch::RiscV => AppConfigRiscV {
                config: app_config.clone(),
            }
            .render(),
        }
        .map_err(|e| anyhow!("Failed to render app linker script: {e}"))
    }
}
