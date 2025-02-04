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

const CDB_FILE_DIRS = [
  '.compile_commands',
  '.pw_ide', // The legacy pw_ide directory
];

// Need this indirection to prevent `workingDir` being called before init.
const CDB_DIRS = () =>
  CDB_FILE_DIRS.map((dir) => path.join(workingDir.get(), dir));

export class Target {
  private _name: string;
  private _dir: string;

  constructor(name: string, dir?: string) {
    this._name = name;
    this._dir = dir ?? CDB_FILE_DIRS[0];
  }

  get name() {
    return this._name;
  }

  get dir() {
    return this._dir;
  }

  get path() {
    return path.join(this._dir, this._name);
  }
}

export async function availableTargets(): Promise<Target[]> {
  return (
    (
      await Promise.all(
        CDB_DIRS().map(async (cwd) =>
          // For each compile commands dir, get the name of every sub dir in the
          // that contains a compile commands file.
          (await glob(`**/${CDB_FILE_NAME}`, { cwd }))
            .map(
              (filePath) =>
                new Target(path.basename(path.dirname(filePath)), cwd),
            )
            // Filter out a catch-all database in the root compile commands dir
            .filter((target) => target.name.trim() !== '.'),
        ),
      )
    )
      .flat()
      // Ensures the targets are returned in alphabetical order by name, which
      // is useful for UI features but annoying to handle at that level.
      .sort((a, b) => a.name.localeCompare(b.name))
  );
}

export function getTarget(): Target | undefined {
  const targetName = settings.codeAnalysisTarget();
  if (targetName === undefined) return undefined;
  return new Target(targetName, settings.codeAnalysisTargetDir());
}

export async function baseSetTarget(target: Target): Promise<void> {
  await settings.codeAnalysisTarget(target.name);
  await settings.codeAnalysisTargetDir(target.dir);
}
