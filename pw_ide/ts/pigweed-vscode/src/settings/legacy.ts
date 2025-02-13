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

import * as fs from 'fs';
import * as fs_p from 'fs/promises';
import * as path from 'path';

import { parse as parseYaml } from 'yaml';
import { z } from 'zod';
import { workingDir } from './vscode';

const DEFAULT_BUILD_DIR_NAME = 'out';
const DEFAULT_TARGET_INFERENCE = '?';
const DEFAULT_WORKSPACE_ROOT = workingDir.get();
const DEFAULT_WORKING_DIR = workingDir.get();
const LEGACY_SETTINGS_FILE_PATH = path.join(workingDir.get(), '.pw_ide.yaml');

const legacySettingsSchema = z.object({
  cascade_settings: z.boolean().default(false),
  clangd_alternate_path: z.string().optional(),
  clangd_additional_query_drivers: z.array(z.string()).default([]),
  compdb_gen_cmd: z.string().optional(),
  compdb_search_paths: z
    .array(z.array(z.string()))
    .default([[DEFAULT_BUILD_DIR_NAME, DEFAULT_TARGET_INFERENCE]]),
  default_target: z.string().optional(),
  // TODO(chadnorvell): Test that this works with a non-default value.
  workspace_root: z.string().default(DEFAULT_WORKSPACE_ROOT),
  // needs some thought
  sync: z.string().optional(),
  targets_exclude: z.array(z.string()).default([]),
  targets_include: z.array(z.string()).default([]),
  target_inference: z.string().default('?'),
  // TODO(chadnorvell): Test that this works with a non-default value.
  working_dir: z.string().default(DEFAULT_WORKING_DIR),
});

type LegacySettings = z.infer<typeof legacySettingsSchema>;

export async function loadLegacySettings(
  settingsData: string,
  onFailureReturnDefault: true,
): Promise<LegacySettings>;

export async function loadLegacySettings(
  settingsData: string,
  onFailureReturnDefault?: false,
): Promise<LegacySettings | null>;

export async function loadLegacySettings(
  settingsData: string,
  onFailureReturnDefault = false,
): Promise<LegacySettings | null> {
  const fallback = onFailureReturnDefault
    ? legacySettingsSchema.parse({})
    : null;

  const parsed = parseYaml(settingsData);
  if (!parsed) return fallback;

  try {
    return legacySettingsSchema.parse(parsed);
  } catch {
    return fallback;
  }
}

export async function loadLegacySettingsFile(): Promise<string | null> {
  if (!fs.existsSync(LEGACY_SETTINGS_FILE_PATH)) return null;
  const data = await fs_p.readFile(LEGACY_SETTINGS_FILE_PATH);
  return data.toString();
}
