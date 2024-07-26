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

import * as fs from 'fs';
import * as path from 'path';

import { glob } from 'glob';

import type { Settings, WorkingDirStore } from './settings';

const PIGWEED_JSON = 'pigweed.json' as const;

/**
 * Find the path to pigweed.json by searching this directory and above.

 * This starts looking in the current working directory, then recursively in
 * each directory above the current working directory, until it finds a
 * pigweed.json file or reaches the file system root. So invoking this anywhere
 * within a Pigweed project directory should work.
 * a path to pigweed.json searching this dir and all parent dirs.
 */
export function findPigweedJsonAbove(workingDir: string): string | null {
  const candidatePath = path.join(workingDir, PIGWEED_JSON);

  // This dir is the Pigweed root. We're done.
  if (fs.existsSync(candidatePath)) return workingDir;

  // We've reached the root of the file system without finding a Pigweed root.
  // We're done, but sadly.
  if (path.dirname(workingDir) === workingDir) return null;

  // Try again in the parent dir.
  return findPigweedJsonAbove(path.dirname(workingDir));
}

/**
 * Find paths to pigweed.json in dirs below this one.
 *
 * This uses a glob search to find all paths to pigweed.json below the current
 * working dir. Usually there shouldn't be more than one.
 */
export async function findPigweedJsonBelow(
  workingDir: string,
): Promise<string[]> {
  const candidatePaths = await glob(`**/${PIGWEED_JSON}`, {
    absolute: true,
    cwd: workingDir,
    // Ignore the source file that pigwed.json is generated from.
    ignore: '**/pw_env_setup/**',
  });

  return candidatePaths.map((p) => path.dirname(p));
}

/**
 * Find the Pigweed root dir within the project.
 *
 * The presence of a pigweed.json file is the sentinel for the Pigweed root.
 * The heuristic is to first search in the current directory or above ("are
 * we inside of a Pigweed directory?"), and failing that, to search in the
 * directories below ("does this project contain a Pigweed directory?").
 *
 * Note that this logic presumes that there's only one Pigweed project
 * directory. In a hypothetical project setup that contained multiple Pigweed
 * projects, this would continue to work when invoked inside of one of those
 * Pigweed directories, but would have inconsistent results when invoked
 * in a parent directory.
 */
export async function inferPigweedProjectRoot(
  workingDir: string,
): Promise<string | null> {
  const rootAbove = findPigweedJsonAbove(workingDir);
  if (rootAbove) return rootAbove;

  const rootsBelow = await findPigweedJsonBelow(workingDir);
  if (rootsBelow) return rootsBelow[0];

  return null;
}

/**
 * Return the path to the Pigweed root dir.
 *
 * If the path is specified in the `pigweed.projectRoot` setting, that path
 * will be returned regardless of whether it actually exists or not, or whether
 * it actually is a Pigweed root dir. We trust the user! But if that setting is
 * not set, we search for the Pigweed root by looking for the presence of
 * pigweed.json.
 */
export async function getPigweedProjectRoot(
  settings: Settings,
  workingDir: WorkingDirStore,
): Promise<string | null> {
  if (!settings.projectRoot()) {
    return inferPigweedProjectRoot(workingDir.get());
  }

  return settings.projectRoot()!;
}

const BOOTSTRAP_SH = 'bootstrap.sh' as const;

export function getBootstrapScript(projectRoot: string): string {
  return path.join(projectRoot, BOOTSTRAP_SH);
}

/** If a project has a bootstrap script, we treat it as a bootstrap project. */
export function isBootstrapProject(projectRoot: string): boolean {
  return fs.existsSync(getBootstrapScript(projectRoot));
}

const WORKSPACE = 'WORKSPACE' as const;
const BZLMOD = 'MODULE.bazel' as const;

export function getWorkspaceFile(projectRoot: string): string {
  return path.join(projectRoot, WORKSPACE);
}

export function getBzlmodFile(projectRoot: string): string {
  return path.join(projectRoot, BZLMOD);
}

/**
 * It's a Bazel project if it has a `WORKSPACE` file or bzlmod file, but not
 * a bootstrap script.
 */
export function isBazelWorkspaceProject(projectRoot: string): boolean {
  return (
    !isBootstrapProject(projectRoot) &&
    (fs.existsSync(getWorkspaceFile(projectRoot)) ||
      fs.existsSync(getBzlmodFile(projectRoot)))
  );
}
