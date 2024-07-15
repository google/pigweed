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

import { glob } from 'glob';
import { basename, dirname, join } from 'path';

import { settingFor, settings, stringSettingFor, workingDir } from './settings';
import { troubleshootingLink } from './links';

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
  return (await glob(`**/${CDB_FILE_NAME}`, { cwd: CDB_DIR() })).map((path) =>
    basename(dirname(path)),
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

function onSetTargetSelection(target: string | undefined) {
  if (target) {
    vscode.window.showInformationMessage(`Analysis target set to: ${target}`);

    setTarget(target);
  }
}

export async function setCompileCommandsTarget() {
  const targets = await availableTargets();

  if (targets.length === 0) {
    vscode.window.showErrorMessage(
      "Couldn't find any targets! Get help at " +
        troubleshootingLink('bazel-no-targets'),
    );

    return;
  }

  vscode.window
    .showQuickPick(targets, {
      title: 'Select a target',
      canPickMany: false,
    })
    .then(onSetTargetSelection);
}
