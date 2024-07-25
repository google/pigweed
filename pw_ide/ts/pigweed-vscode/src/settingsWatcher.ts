// Copyright 2023 The Pigweed Authors
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
import { getSettingsData, syncSettingsSharedToProject } from './configParsing';
import { writeClangdSettingsFile } from './clangd';
import { settings } from './settings';
import logger from './logging';

export function initSettingsFilesWatcher(): { dispose: () => void } {
  const workspaceFolders = vscode.workspace.workspaceFolders;
  if (!workspaceFolders) return { dispose: () => null };

  const workspaceFolder = workspaceFolders[0];

  logger.info('Initializing settings file watcher');

  const watcher = vscode.workspace.createFileSystemWatcher(
    new vscode.RelativePattern(workspaceFolder, '.vscode/settings.shared.json'),
  );

  watcher.onDidChange(async () => {
    logger.info('[onDidChange] triggered from settings file watcher');
    syncSettingsSharedToProject(await getSettingsData());
  });

  watcher.onDidCreate(async () => {
    logger.info('[onDidCreate] triggered from settings file watcher');
    syncSettingsSharedToProject(await getSettingsData());
  });

  watcher.onDidDelete(async () => {
    logger.info('[onDidDelete] triggered from settings file watcher');
    syncSettingsSharedToProject(await getSettingsData());
  });

  return {
    dispose: () => watcher.dispose(),
  };
}

async function handleClangdFileEvent() {
  const target = settings.codeAnalysisTarget();

  if (target) {
    await writeClangdSettingsFile(target);
  }
}

export function initClangdFileWatcher(): { dispose: () => void } {
  const workspaceFolders = vscode.workspace.workspaceFolders;
  if (!workspaceFolders) return { dispose: () => null };

  const workspaceFolder = workspaceFolders[0];

  logger.info('Initializing clangd file watcher');

  const watcher = vscode.workspace.createFileSystemWatcher(
    new vscode.RelativePattern(workspaceFolder, '.clangd.shared'),
  );

  watcher.onDidChange(async () => {
    logger.info('[onDidChange] triggered from clangd file watcher');
    await handleClangdFileEvent();
  });

  watcher.onDidCreate(async () => {
    logger.info('[onDidCreate] triggered from clangd file watcher');
    await handleClangdFileEvent();
  });

  watcher.onDidDelete(async () => {
    logger.info('[onDidDelete] triggered from clangd file watcher');
    await handleClangdFileEvent();
  });

  return {
    dispose: () => watcher.dispose(),
  };
}
