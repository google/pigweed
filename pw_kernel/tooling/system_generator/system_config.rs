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

use hashlink::LinkedHashMap;
use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct SystemConfig {
    pub arch: String,
    pub kernel: KernelConfig,
    #[serde(default)]
    pub apps: LinkedHashMap<String, AppConfig>,
    #[serde(skip_deserializing)]
    pub arch_crate_name: &'static str,
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct KernelConfig {
    pub flash_start_address: usize,
    pub flash_size_bytes: usize,
    pub ram_start_address: usize,
    pub ram_size_bytes: usize,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct AppConfig {
    pub flash_size_bytes: usize,
    pub ram_size_bytes: usize,
    pub process: ProcessConfig,
    // The following fields are calculated, not defined by a user.
    // TODO: davidroth - if this becomes too un-wieldy, we should
    // split the config schema from the template structs.
    #[serde(skip_deserializing)]
    pub flash_start_address: usize,
    #[serde(skip_deserializing)]
    pub flash_end_address: usize,
    #[serde(skip_deserializing)]
    pub ram_start_address: usize,
    #[serde(skip_deserializing)]
    pub ram_end_address: usize,
    #[serde(skip_deserializing)]
    pub start_fn_address: usize,
    #[serde(skip_deserializing)]
    pub initial_sp: usize,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct ProcessConfig {
    name: String,
    #[serde(default)]
    objects: LinkedHashMap<String, ObjectConfig>,
    threads: Vec<ThreadConfig>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(tag = "type")]
#[serde(rename_all = "snake_case")]
pub enum ObjectConfig {
    Ticker(TickerConfig),
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct TickerConfig;

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct ThreadConfig {
    name: String,
    stack_size_bytes: usize,
}
