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
import { ExtensionContext } from 'vscode';

import {
  interactivelySetBazeliskPath,
  setBazelRecommendedSettings,
} from '../bazel';

import {
  BazelRefreshCompileCommandsWatcher,
  showProgressDuringRefresh,
} from '../bazelWatcher';

import {
  ClangdActiveFilesCache,
  disableInactiveFileCodeIntelligence,
  enableInactiveFileCodeIntelligence,
  refreshCompileCommandsAndSetTarget,
  setCompileCommandsTarget,
} from '../clangd';

import { RefreshManager } from '../refreshManager';
import { patchBazeliskIntoTerminalPath } from '../terminal';

export function registerBazelProjectCommands(
  context: ExtensionContext,
  refreshManager: RefreshManager<any>,
  compileCommandsWatcher: BazelRefreshCompileCommandsWatcher,
  clangdActiveFilesCache: ClangdActiveFilesCache,
) {
  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.disable-inactive-file-code-intelligence',
      () => disableInactiveFileCodeIntelligence(clangdActiveFilesCache),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.enable-inactive-file-code-intelligence',
      () => enableInactiveFileCodeIntelligence(clangdActiveFilesCache),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.refresh-compile-commands', () => {
      compileCommandsWatcher.refresh();
      showProgressDuringRefresh(refreshManager);
    }),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.refresh-compile-commands-and-set-target',
      () =>
        refreshCompileCommandsAndSetTarget(
          compileCommandsWatcher.refresh,
          refreshManager,
          clangdActiveFilesCache,
        ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.set-bazelisk-path',
      interactivelySetBazeliskPath,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.set-bazel-recommended-settings',
      setBazelRecommendedSettings,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.select-target', () =>
      setCompileCommandsTarget(clangdActiveFilesCache),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.launch-terminal', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bazel projects',
      ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.bootstrap-terminal', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bazel projects',
      ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.activate-bazelisk-in-terminal',
      patchBazeliskIntoTerminalPath,
    ),
  );
}
