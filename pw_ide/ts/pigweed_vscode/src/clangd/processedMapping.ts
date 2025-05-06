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
 * Store and retrieve the project types for processed targets.
 */

import * as fs_p from 'fs/promises';
import * as path from 'path';

import { CDB_FILE_DIR } from './paths';
import logger from '../logging';
import { workingDir } from '../settings/vscode';
import { loadUnprocessedMapping } from './unprocessedMapping';

export const PROCESSED_MAPPING_FILE = 'processed_targets.json';

export interface ProcessedTargetMapping {
  [targetName: string]: 'bazel' | 'bootstrap';
}

function getMappingFilePath(): string {
  return path.join(workingDir.get(), CDB_FILE_DIR, PROCESSED_MAPPING_FILE);
}

export async function saveProcessedMapping(
  processedCompDbs: ProcessedTargetMapping,
): Promise<void> {
  const filePath = getMappingFilePath();
  await fs_p.mkdir(path.dirname(filePath), { recursive: true });
  await fs_p.writeFile(filePath, JSON.stringify(processedCompDbs, null, 2));
}

export async function loadProcessedMapping(): Promise<ProcessedTargetMapping> {
  const filePath = getMappingFilePath();

  try {
    const content = await fs_p.readFile(filePath, 'utf8');
    return JSON.parse(content) as ProcessedTargetMapping;
  } catch (error) {
    // If the file doesn't exist, return an empty mapping.
    if ((error as NodeJS.ErrnoException).code === 'ENOENT') {
      return {};
    }

    // For other errors, log and return an empty mapping
    logger.error(`Error loading processed target mapping: ${error}`);
    return {};
  }
}

export async function getTargetType(
  targetName: string,
): Promise<'bazel' | 'bootstrap'> {
  const processed = await loadProcessedMapping();
  const unprocessed = await loadUnprocessedMapping();

  // If the target is unprocessed, by definition it isn't a Bazel target.
  if (targetName in unprocessed) return 'bootstrap';

  // If there's an explicit entry in the processed mapping, return it.
  // In practice, this will only contain bootstrap targets.
  if (processed[targetName]) return processed[targetName];

  // If it doesn't appear in either file, we assume it's a Bazel target.
  return 'bazel';
}
