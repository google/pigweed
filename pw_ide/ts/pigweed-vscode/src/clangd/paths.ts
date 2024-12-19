// Copyright 2024 The Pigweed Authors
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

import * as path from 'path';

import { glob } from 'glob';

import { settings, workingDir } from '../settings';

const CDB_FILE_NAME = 'compile_commands.json' as const;
const CDB_FILE_DIR = '.compile_commands' as const;

// Need this indirection to prevent `workingDir` being called before init.
const CDB_DIR = () => path.join(workingDir.get(), CDB_FILE_DIR);

export const targetPath = (target: string) => path.join(`${CDB_DIR()}`, target);
export const targetCompileCommandsPath = (target: string) =>
  path.join(targetPath(target), CDB_FILE_NAME);

export async function availableTargets(): Promise<string[]> {
  // Get the name of every sub dir in the compile commands dir that contains
  // a compile commands file.
  return (
    (await glob(`**/${CDB_FILE_NAME}`, { cwd: CDB_DIR() }))
      .map((filePath) => path.basename(path.dirname(filePath)))
      // Filter out a catch-all database in the root compile commands dir
      .filter((name) => name.trim() !== '.')
  );
}

export function getTarget(): string | undefined {
  return settings.codeAnalysisTarget();
}
