// Copyright 2025 The Pigweed Authors
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
import { Disposable } from '../disposables';
import logger from '../logging';
import { CDB_FILE_DIR, CDB_FILE_NAME } from './paths';
import path from 'path';
import { workingDir } from '../settings/vscode';
import { ClangdActiveFilesCache } from './activeFilesCache';
import { RefreshManager } from '../refreshManager';
import { restartClangd } from './vscCommands';

/** A file watcher that automatically runs a refresh on Bazel file changes. */
export class CompileCommandsWatcher extends Disposable {
  constructor(
    private refreshManager: RefreshManager<any>,
    private filesCache: ClangdActiveFilesCache,
  ) {
    super();
    logger.info('Initializing compile commands file watcher');
    const watcher = vscode.workspace.createFileSystemWatcher(
      new vscode.RelativePattern(
        path.join(workingDir.get(), CDB_FILE_DIR),
        path.join('**', CDB_FILE_NAME),
      ),
    );

    watcher.onDidChange(() => {
      logger.info('[onDidChange] triggered from compile commands watcher');
      this.refresh();
    });

    watcher.onDidCreate(() => {
      logger.info('[onDidCreate] triggered from compile commands watcher');
      this.refresh();
    });

    watcher.onDidDelete(() => {
      logger.info('[onDidDelete] triggered from compile commands watcher');
      this.refresh();
    });

    this.disposables.push(watcher);
  }

  private debounceTimeout: NodeJS.Timeout | undefined;

  /** We want to do a few things when compile_commands directory is touched:
   * - Restart the clangd server so it picks up the new compile commands.
   * - Re-set the target if current one doesn't exist anymore ie.
   * platforms changed in new build etc.
   */
  refresh = () => {
    if (this.debounceTimeout) {
      clearTimeout(this.debounceTimeout);
    }

    this.debounceTimeout = setTimeout(() => {
      restartClangd();
      this.filesCache.refresh();
      this.refreshManager.refresh();
    }, 2000);
  };
}
