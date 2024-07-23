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

import * as vscode from 'vscode';

import { createInterface } from 'readline/promises';
import { createReadStream } from 'fs';
import { glob } from 'glob';
import { basename, dirname, isAbsolute, join } from 'path';

import { refreshCompileCommands } from './bazelWatcher';
import { OK, RefreshCallback, refreshManager } from './refreshManager';
import { settingFor, settings, stringSettingFor, workingDir } from './settings';
import { launchTroubleshootingLink } from './links';
import logger from './logging';

const CDB_FILE_NAME = 'compile_commands.json' as const;
const CDB_FILE_DIR = '.compile_commands' as const;

// Need this indirection to prevent `workingDir` being called before init.
const CDB_DIR = () => join(workingDir.get(), CDB_FILE_DIR);

// TODO: https://pwbug.dev/352601321 - This is brittle and also probably
// doesn't work on Windows.
const clangdPath = () =>
  join(workingDir.get(), 'external', 'llvm_toolchain', 'bin', 'clangd');

export const targetPath = (target: string) => join(`${CDB_DIR()}`, target);
export const targetCompileCommandsPath = (target: string) =>
  join(targetPath(target), CDB_FILE_NAME);

export async function availableTargets(): Promise<string[]> {
  // Get the name of every sub dir in the compile commands dir that contains
  // a compile commands file.
  return (
    (await glob(`**/${CDB_FILE_NAME}`, { cwd: CDB_DIR() }))
      .map((path) => basename(dirname(path)))
      // Filter out a catch-all database in the root compile commands dir
      .filter((name) => name.trim() !== '.')
  );
}

export function getTarget(): string | undefined {
  return settings.codeAnalysisTarget();
}

export async function setTarget(target: string): Promise<void> {
  if (!(await availableTargets()).includes(target)) {
    throw new Error(`Target not among available targets: ${target}`);
  }

  settings.codeAnalysisTarget(target);

  const { update: updatePath } = stringSettingFor('path', 'clangd');
  const { update: updateArgs } = settingFor<string[]>('arguments', 'clangd');

  // These updates all happen asynchronously, and we want to make sure they're
  // all done before we trigger a clangd restart.
  Promise.all([
    updatePath(clangdPath()),
    updateArgs([
      `--compile-commands-dir=${targetPath(target)}`,
      '--query-driver=**',
      '--header-insertion=never',
      '--background-index',
    ]),
  ]).then(() =>
    // Restart the clangd server so it picks up the new setting.
    vscode.commands.executeCommand('clangd.restart'),
  );
}

/** Parse a compilation database and get the source files in the build. */
async function parseForSourceFiles(target: string): Promise<Set<string>> {
  const rd = createInterface({
    input: createReadStream(targetCompileCommandsPath(target)),
    crlfDelay: Infinity,
  });

  const regex = /^\s*"file":\s*"([^"]*)",$/;
  const files = new Set<string>();

  for await (const line of rd) {
    const match = regex.exec(line);

    if (match) {
      const matchedPath = match[1];

      if (
        // Ignore files outside of this project dir
        !isAbsolute(matchedPath) &&
        // Ignore build artifacts
        !matchedPath.startsWith('bazel') &&
        // Ignore external dependencies
        !matchedPath.startsWith('external')
      ) {
        files.add(matchedPath);
      }
    }
  }

  return files;
}

/** A cache of files that are in the builds of each target. */
let activeFiles: Record<string, Set<string>> = {};

export const refreshActiveFiles: RefreshCallback = async () => {
  logger.info('Refreshing active files cache');
  const targets = await availableTargets();

  const targetSourceFiles = await Promise.all(
    targets.map(
      async (target) => [target, await parseForSourceFiles(target)] as const,
    ),
  );

  activeFiles = Object.fromEntries(targetSourceFiles);
  logger.info('Finished refreshing active files cache');
  return OK;
};

// Refresh the active files cache after refreshing compile commands
refreshManager.on(refreshActiveFiles, 'didRefresh');

/** Get the active files for a particular target. */
export async function getActiveFiles(target: string): Promise<Set<string>> {
  if (!Object.keys(activeFiles).includes(target)) {
    return new Set();
  }

  return activeFiles[target];
}

const setCompileCommandsCallbacks: ((target: string) => void)[] = [];

/**
 * Register callbacks to be called when the target is changed.
 *
 * Setting the target does persist the target into settings, where it can be
 * retrieved by other functions. But there's enough asyncronicity in that
 * procedure that it's more reliable to just get the target name directly, so
 * we provide it to the callbacks.
 */
export function onSetCompileCommands(cb: (target: string) => void) {
  setCompileCommandsCallbacks.push(cb);
}

async function onSetTargetSelection(target: string | undefined) {
  if (target) {
    vscode.window.showInformationMessage(`Analysis target set to: ${target}`);
    await setTarget(target);
    setCompileCommandsCallbacks.forEach((cb) => cb(target));
  }
}

export async function setCompileCommandsTarget() {
  const targets = await availableTargets();

  if (targets.length === 0) {
    vscode.window
      .showErrorMessage("Couldn't find any targets!", 'Get Help')
      .then((selection) => {
        switch (selection) {
          case 'Get Help': {
            launchTroubleshootingLink('bazel-no-targets');
            break;
          }
        }
      });

    return;
  }

  vscode.window
    .showQuickPick(targets, {
      title: 'Select a target',
      canPickMany: false,
    })
    .then(onSetTargetSelection);
}

export async function refreshCompileCommandsAndSetTarget() {
  await refreshCompileCommands();
  await refreshManager.waitFor('didRefresh');
  await setCompileCommandsTarget();
}
