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
import { RelativePattern } from 'vscode';

import { Disposable } from '../disposables';
import { ClangdActiveFilesCache } from '../clangd';
import { getSettingsData, syncSettingsSharedToProject } from '../configParsing';
import logger from '../logging';
import { settings } from './vscode';

export class SettingsFileWatcher extends Disposable {
  constructor() {
    super();

    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) return;

    const workspaceFolder = workspaceFolders[0];

    logger.info('Initializing settings file watcher');

    const watcher = vscode.workspace.createFileSystemWatcher(
      new RelativePattern(workspaceFolder, '.vscode/settings.shared.json'),
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

    this.disposables.push(watcher);
  }
}

async function handleClangdFileEvent(
  settingsFileWriter: (target: string) => Promise<void>,
) {
  const target = settings.codeAnalysisTarget();

  if (target) {
    await settingsFileWriter(target);
  }
}

export class ClangdFileWatcher extends Disposable {
  constructor(clangdActiveFilesCache: ClangdActiveFilesCache) {
    super();

    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) return;

    const workspaceFolder = workspaceFolders[0];

    logger.info('Initializing clangd file watcher');

    const watcher = vscode.workspace.createFileSystemWatcher(
      new RelativePattern(workspaceFolder, '.clangd.shared'),
    );

    watcher.onDidChange(async () => {
      logger.info('[onDidChange] triggered from clangd file watcher');
      await handleClangdFileEvent(clangdActiveFilesCache.writeToSettings);
    });

    watcher.onDidCreate(async () => {
      logger.info('[onDidCreate] triggered from clangd file watcher');
      await handleClangdFileEvent(clangdActiveFilesCache.writeToSettings);
    });

    watcher.onDidDelete(async () => {
      logger.info('[onDidDelete] triggered from clangd file watcher');
      await handleClangdFileEvent(clangdActiveFilesCache.writeToSettings);
    });

    this.disposables.push(watcher);
  }
}
