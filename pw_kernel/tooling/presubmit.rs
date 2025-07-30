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

use std::collections::HashMap;
use std::io::BufReader;
use std::path::PathBuf;
use std::process;

use anyhow::{Result, anyhow};
use cliclack::{intro, outro, outro_cancel, spinner};
use serde::Deserialize;

#[derive(Clone, Debug, Deserialize)]
pub struct BazelPrograms {
    pub default: Vec<Vec<String>>,
}

#[derive(Clone, Debug, Deserialize)]
pub struct BazelPresubmit {
    pub remote_cache: bool,
    pub upload_local_results: bool,
    pub programs: HashMap<String, Vec<Vec<String>>>,
}

#[derive(Clone, Debug, Deserialize)]
pub struct PigweedConfig {
    pub bazel_presubmit: BazelPresubmit,
}

#[derive(Clone, Debug, Deserialize)]
pub struct PigweedProject {
    pub pw: PigweedConfig,
}

fn load_project_config() -> Result<PigweedProject> {
    let workspace_dir = std::env::var("BUILD_WORKSPACE_DIRECTORY")
        .map_err(|_| anyhow!("BUILD_WORKSPACE_DIRECTORY not found.  Please run inside bazel."))?;
    let path: PathBuf = [&workspace_dir, "pigweed.json"].iter().collect();
    let reader = BufReader::new(std::fs::File::open(path)?);

    let project_config = serde_json::from_reader(reader)?;
    Ok(project_config)
}

fn run_bazel_presubmit(args: &Vec<String>) -> Result<()> {
    let workspace_dir = std::env::var("BUILD_WORKSPACE_DIRECTORY")
        .map_err(|_| anyhow!("BUILD_WORKSPACE_DIRECTORY not found.  Please run inside bazel."))?;
    let spinner = spinner();
    spinner.start(format!("Running {args:?}..."));
    let output = process::Command::new("bazelisk")
        .current_dir(workspace_dir)
        .args(args.clone())
        .output()?;

    if !output.status.success() {
        return Err(anyhow!(
            "{args:?} failed:\n{}",
            String::from_utf8_lossy(&output.stderr)
        ));
    }
    spinner.stop(format!("{args:?} success."));

    Ok(())
}

fn run_presubmit() -> Result<()> {
    intro("Running presubmits")?;

    let config =
        load_project_config().map_err(|e| anyhow!("Failed to load project config: {e}"))?;

    let Some(kernel_program) = config.pw.bazel_presubmit.programs.get("kernel") else {
        return Err(anyhow!("No `kernel` program in projects bazel_presubmit"));
    };

    for args in kernel_program {
        run_bazel_presubmit(args)?;
    }

    outro("All presubmits succeeded")?;
    Ok(())
}

fn main() {
    if let Err(e) = run_presubmit() {
        outro_cancel(format!("{e}")).expect("outro succeeds");
        std::process::exit(1);
    }
}
