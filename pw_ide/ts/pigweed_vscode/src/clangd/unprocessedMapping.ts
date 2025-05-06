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

/**
 * Store and retrieve compilation database paths for unprocessed targets.
 *
 * Compilation databases that are not directly generated in the canonical
 * compile commands directory (like we do with Bazel) and are not processed to
 * produce new files in the compile commands directory (like we do with GN) do
 * not need to be moved from their original location to the canonical directory.
 * In fact, it's advantageous to leave them in place, because then `clangd` can
 * get refreshed compile commands information as soon as the build system
 * produces it, without an intermediate generation or processing step. This is
 * how our CMake integration works.
 *
 * So instead of finding code intelligence targets by looking for files in the
 * canonical compile commands directory, we look at a mapping file that relates
 * a target name to a direct path to a compilation database.
 */

import * as fs_p from 'fs/promises';
import * as path from 'path';

import { CDB_FILE_DIR } from './paths';
import logger from '../logging';
import { workingDir } from '../settings/vscode';

export const UNPROCESSED_MAPPING_FILE = 'unprocessed_targets.json';

export interface UnprocessedTargetMapping {
  // Key is the target name, value is the absolute path to the compDB file.
  [targetName: string]: string;
}

function getMappingFilePath(): string {
  return path.join(workingDir.get(), CDB_FILE_DIR, UNPROCESSED_MAPPING_FILE);
}

export async function saveUnprocessedMapping(
  unprocessedCompDbs: [string, string][],
): Promise<void> {
  const mapping: UnprocessedTargetMapping =
    Object.fromEntries(unprocessedCompDbs);

  const filePath = getMappingFilePath();
  await fs_p.mkdir(path.dirname(filePath), { recursive: true });
  await fs_p.writeFile(filePath, JSON.stringify(mapping, null, 2));

  logger.info(
    `Saved ${unprocessedCompDbs.length} target mappings to ${filePath}`,
  );
}

export async function loadUnprocessedMapping(): Promise<UnprocessedTargetMapping> {
  const filePath = getMappingFilePath();

  try {
    const content = await fs_p.readFile(filePath, 'utf8');
    return JSON.parse(content) as UnprocessedTargetMapping;
  } catch (error) {
    // If the file doesn't exist, return an empty mapping.
    if ((error as NodeJS.ErrnoException).code === 'ENOENT') {
      return {};
    }

    // For other errors, log and return an empty mapping
    logger.error(`Error loading unprocessed target mapping: ${error}`);
    return {};
  }
}
