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

use std::fs;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;

use anyhow::{Context, Result, anyhow};
use clap::{Args, Parser, Subcommand};
use minijinja::{Environment, State};
use serde::Serialize;
use serde::de::DeserializeOwned;

pub mod system_config;

use system_config::SystemConfig;

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
    TargetCodegen,
    TargetLinkerScript,
    AppLinkerScript(AppLinkerScriptArgs),
}

#[derive(Args, Debug)]
pub struct AppLinkerScriptArgs {
    #[arg(long, required = true)]
    pub app_name: String,
}

pub trait ArchConfigInterface {
    fn get_arch_crate_name(&self) -> &'static str;
    fn get_start_fn_address(&self, flash_start_address: usize) -> usize;
}

pub fn parse_config<A: ArchConfigInterface + DeserializeOwned>(
    cli: &Cli,
) -> Result<system_config::SystemConfig<A>> {
    let json5_str =
        fs::read_to_string(&cli.common_args.config).context("Failed to read config file")?;
    let mut config: SystemConfig<A> =
        serde_json5::from_str(&json5_str).context("Failed to parse config file")?;
    config.calculate_and_validate()?;
    Ok(config)
}

const FLASH_ALIGNMENT: usize = 4;
const RAM_ALIGNMENT: usize = 8;

impl ArchConfigInterface for system_config::Armv8MConfig {
    fn get_arch_crate_name(&self) -> &'static str {
        "arch_arm_cortex_m"
    }

    fn get_start_fn_address(&self, flash_start_address: usize) -> usize {
        // On Armv8M, the +1 is to denote thumb mode.
        flash_start_address + 1
    }
}

impl ArchConfigInterface for system_config::RiscVConfig {
    fn get_arch_crate_name(&self) -> &'static str {
        "arch_riscv"
    }

    fn get_start_fn_address(&self, flash_start_address: usize) -> usize {
        flash_start_address
    }
}

pub struct SystemGenerator<'a, A: ArchConfigInterface> {
    cli: Cli,
    config: system_config::SystemConfig<A>,
    env: Environment<'a>,
}

impl<'a, A: ArchConfigInterface + Serialize> SystemGenerator<'a, A> {
    pub fn new(cli: Cli, config: system_config::SystemConfig<A>) -> Result<Self> {
        let mut instance = Self {
            cli,
            config,
            env: Environment::new(),
        };

        instance.env.add_filter("hex", hex);
        for (name, path) in instance.cli.common_args.templates.clone() {
            let template = fs::read_to_string(path)?;
            instance.env.add_template_owned(name, template)?;
        }

        instance.populate_addresses();

        Ok(instance)
    }

    pub fn generate(&mut self) -> Result<()> {
        let out_str = match &self.cli.command {
            Command::TargetCodegen => self.render_system()?,
            Command::TargetLinkerScript => self.render_system_linker_script()?,
            Command::AppLinkerScript(args) => self.render_app_linker_script(&args.app_name)?,
        };

        let mut file = File::create(&self.cli.common_args.output)?;
        file.write_all(out_str.as_bytes())
            .context("Failed to write output")
    }

    fn render_system(&self) -> Result<String> {
        let template = self.env.get_template("system")?;
        match template.render(&self.config) {
            Ok(str) => Ok(str),
            Err(e) => Err(anyhow!(e)),
        }
    }

    fn render_system_linker_script(&self) -> Result<String> {
        let template = self.env.get_template("system")?;
        match template.render(&self.config) {
            Ok(str) => Ok(str),
            Err(e) => Err(anyhow!(e)),
        }
    }

    fn render_app_linker_script(&self, app_name: &String) -> Result<String> {
        let template = self.env.get_template("app")?;
        match template.render(self.config.apps.get(app_name)) {
            Ok(str) => Ok(str),
            Err(e) => Err(anyhow!(e)),
        }
    }

    #[must_use]
    fn align(value: usize, alignment: usize) -> usize {
        debug_assert!(alignment.is_power_of_two());
        (value + alignment - 1) & !(alignment - 1)
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

        self.config.arch_crate_name = self.config.arch.get_arch_crate_name();

        for app in self.config.apps.values_mut() {
            app.flash_start_address = next_flash_start_address;
            next_flash_start_address = Self::align(
                app.flash_start_address + app.flash_size_bytes,
                FLASH_ALIGNMENT,
            );

            app.ram_start_address = next_ram_start_address;
            next_ram_start_address =
                Self::align(app.ram_start_address + app.ram_size_bytes, RAM_ALIGNMENT);

            app.start_fn_address = self
                .config
                .arch
                .get_start_fn_address(app.flash_start_address);

            app.initial_sp = app.ram_start_address + app.ram_size_bytes;
        }
    }
}

// Custom filters
#[must_use]
pub fn hex(_state: &State, value: usize) -> String {
    format!("{value:#x}")
}
