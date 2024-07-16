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

import { OK, RefreshStatus, refreshManager } from './refreshManager';
import { settings } from './settings';

const statusBarItem = vscode.window.createStatusBarItem(
  vscode.StatusBarAlignment.Left,
  100,
);

export function getStatusBarItem(): vscode.StatusBarItem {
  statusBarItem.command = 'pigweed.select-target';
  updateStatusBarItem();
  statusBarItem.show();
  return statusBarItem;
}

function icon(status: RefreshStatus) {
  switch (status) {
    case 'idle':
      return '$(check)';
    case 'fault':
      return '$(warning)';
    default:
      return '$(sync~spin)';
  }
}

function command(status: RefreshStatus) {
  switch (status) {
    case 'idle':
      return 'pigweed.select-target';
    case 'fault':
      return 'pigweed.refresh-compile-commands-and-set-target';
    default:
      return 'pigweed.open-output-panel';
  }
}

export function updateStatusBarItem(target?: string) {
  const targetText =
    target ?? settings.codeAnalysisTarget() ?? 'Select a Target';

  statusBarItem.text = `${icon(refreshManager.state)} ${targetText}`;
  statusBarItem.command = command(refreshManager.state);
  return OK;
}
