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

import { OK, refreshManager } from './refreshManager';
import { settings } from './settings';

const targetStatusBarItem = vscode.window.createStatusBarItem(
  vscode.StatusBarAlignment.Left,
  100,
);

export function getTargetStatusBarItem(): vscode.StatusBarItem {
  targetStatusBarItem.command = 'pigweed.select-target';
  updateTargetStatusBarItem();
  targetStatusBarItem.show();
  return targetStatusBarItem;
}

export function updateTargetStatusBarItem(target?: string) {
  const status = refreshManager.state;

  const targetText =
    target ?? settings.codeAnalysisTarget() ?? 'Select a Target';

  const text = (icon: string) => `${icon} ${targetText}`;

  switch (status) {
    case 'idle':
      targetStatusBarItem.tooltip = 'Click to select a code analysis target';
      targetStatusBarItem.text = text('$(check)');
      targetStatusBarItem.command = 'pigweed.select-target';
      break;
    case 'fault':
      targetStatusBarItem.tooltip = 'An error occurred! Click to try again';
      targetStatusBarItem.text = text('$(warning)');

      targetStatusBarItem.command =
        'pigweed.refresh-compile-commands-and-set-target';

      break;
    default:
      targetStatusBarItem.tooltip =
        'Refreshing compile commands. Click to open the output panel';

      targetStatusBarItem.text = text('$(sync~spin)');
      targetStatusBarItem.command = 'pigweed.open-output-panel';
      break;
  }

  return OK;
}

const inactiveVisibilityStatusBarItem = vscode.window.createStatusBarItem(
  vscode.StatusBarAlignment.Left,
  99,
);

export function getInactiveVisibilityStatusBarItem(): vscode.StatusBarItem {
  updateInactiveVisibilityStatusBarItem();
  inactiveVisibilityStatusBarItem.show();
  return inactiveVisibilityStatusBarItem;
}

export function updateInactiveVisibilityStatusBarItem() {
  if (settings.disableInactiveFileCodeIntelligence()) {
    inactiveVisibilityStatusBarItem.tooltip =
      'Code intelligence is disabled for files not in current ' +
      "target's build. Click to enable.";

    inactiveVisibilityStatusBarItem.text = '$(eye-closed)';

    inactiveVisibilityStatusBarItem.command =
      'pigweed.enable-inactive-file-code-intelligence';
  } else {
    inactiveVisibilityStatusBarItem.tooltip =
      'Code intelligence is enabled for all files.' +
      "Click to disable for files not in current target's build.";

    inactiveVisibilityStatusBarItem.text = '$(eye)';

    inactiveVisibilityStatusBarItem.command =
      'pigweed.disable-inactive-file-code-intelligence';
  }
}
