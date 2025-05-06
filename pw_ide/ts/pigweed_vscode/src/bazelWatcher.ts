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
import { ProgressLocation } from 'vscode';

import { Disposable } from './disposables';
import { launchTroubleshootingLink } from './links';
import logger from './logging';
import { getPigweedProjectRoot } from './project';
import { getReliableBazelExecutable } from './bazel';

import {
  RefreshCallback,
  OK,
  RefreshManager,
  RefreshCallbackResult,
} from './refreshManager';

import { bazel_executable, settings, workingDir } from './settings/vscode';
import { generateCompileCommands } from './clangd/compileCommandsGenerator';
import { CDB_FILE_DIR, CDB_FILE_NAME } from './clangd';

/** Regex for finding ANSI escape codes. */
const ANSI_PATTERN = new RegExp(
  '[\\u001B\\u009B][[\\]()#;?]*(?:(?:(?:(?:;[-a-zA-Z\\d\\/#&.:=?%@~_]+)' +
    '*|[a-zA-Z\\d]+(?:;[-a-zA-Z\\d\\/#&.:=?%@~_]*)*)?\\u0007)' +
    '|(?:(?:\\d{1,4}(?:;\\d{0,4})*)?[\\dA-PR-TZcf-nq-uy=><~]))',
  'g',
);

/** Strip ANSI escape codes from a string. */
const stripAnsi = (input: string): string => input.replace(ANSI_PATTERN, '');

/** Remove ANSI escape codes that aren't supported in the output window. */
const cleanLogLine = (line: Buffer) => {
  const stripped = stripAnsi(line.toString());

  // Remove superfluous newlines
  if (stripped.at(-1) === '\n') {
    return stripped.substring(0, stripped.length - 1);
  }

  return stripped;
};

/** A file watcher that automatically runs a refresh on Bazel file changes. */
export class BazelRefreshCompileCommandsWatcher extends Disposable {
  private refreshManager: RefreshManager<any>;

  constructor(refreshManager: RefreshManager<any>, disable = false) {
    super();

    this.refreshManager = refreshManager;
    if (disable) return;

    logger.info('Initializing Bazel refresh compile commands file watcher');

    const watchers = [
      vscode.workspace.createFileSystemWatcher('**/WORKSPACE'),
      vscode.workspace.createFileSystemWatcher('**/*.bazel'),
      vscode.workspace.createFileSystemWatcher('**/*.bzl'),
    ];

    watchers.forEach((watcher) => {
      watcher.onDidChange(() => {
        logger.info(
          '[onDidChange] triggered from refresh compile commands watcher',
        );
        this.refresh();
      });

      watcher.onDidCreate(() => {
        logger.info(
          '[onDidCreate] triggered from refresh compile commands watcher',
        );
        this.refresh();
      });

      watcher.onDidDelete(() => {
        logger.info(
          '[onDidDelete] triggered from refresh compile commands watcher',
        );
        this.refresh();
      });
    });

    this.disposables.push(...watchers);
  }

  /** Trigger a refresh compile commands process. */
  refresh = () => {
    this.refreshManager.onOnce(async () => {
      const cwd = (await getPigweedProjectRoot(settings, workingDir)) as string;
      const bazelCmd = getReliableBazelExecutable();
      if (!bazelCmd) {
        throw new Error("Couldn't find a Bazel or Bazelisk executable");
      }
      await generateCompileCommands(
        bazelCmd,
        cwd,
        CDB_FILE_DIR,
        CDB_FILE_NAME,
        ['//...'],
        [],
        logger,
      );
      // Restart the clangd server so it picks up the new compile commands.
      vscode.commands.executeCommand('clangd.restart');
      return OK;
    }, 'refreshing');
    this.refreshManager.refresh();
  };
}

/** Show an informative progress indicator when refreshing. */
export async function showProgressDuringRefresh(
  refreshManager: RefreshManager<any>,
) {
  return vscode.window.withProgress(
    {
      location: ProgressLocation.Notification,
      cancellable: true,
    },
    async (progress, token) => {
      progress.report({
        message: 'Refreshing code intelligence data...',
      });

      // If it takes a while, notify the user that this is normal.
      setTimeout(
        () =>
          progress.report({
            message:
              'Refreshing code intelligence data... ' +
              "This can take a while, but it's still working.",
          }),
        5000,
      );

      // Clicking cancel will send the abort signal.
      token.onCancellationRequested(() => refreshManager.abort());

      // Indicate that we're actually done refreshing compile commands, and now
      // we're updating the active files cache. This is also a multi-seconds
      // long process, but doesn't produce any output in the interim, so it's
      // helpful to be clear that something is happening.
      refreshManager.on(
        () => {
          progress.report({
            message: 'Refreshing active files cache...',
          });

          // If it takes a while, notify the user that this is normal.
          setTimeout(
            () =>
              progress.report({
                message: 'Refreshing active files cache... Almost done!',
              }),
            15000,
          );

          return OK;

          // This is kind of an unfortunate load-bearing hack.
          // Shouldn't registering this just to 'didRefresh' work?
          //
          // Yes, but:
          //   - Refresh manager callbacks registered to the same state run
          //     strictly in order of registration
          //   - This callback will be registered *after* the active files cache
          //     refresh callback, which is also registered to 'didRefresh'
          //   - So this would be called only after the active files cache
          //     refresh was done, which is obviously not what we want.
          //
          // So to ensure that this runs first, it takes advantage of the purely
          // incidental fact that the more specific 'refreshing->didRefresh'
          // callbacks are run before the less specific 'didRefresh' callbacks.
          // So this works, but it's a bad design that should be fixed.
          // TODO: https://pwbug.dev/357720042 - See above
        },
        'didRefresh',
        'refreshing',
      );

      return new Promise<void>((resolve) => {
        // On abort, complete the progress bar, notify that it was aborted.
        refreshManager.on(() => {
          vscode.window.showInformationMessage(
            'Aborted refreshing code intelligence data!',
          );
          resolve();
          return OK;
        }, 'abort');

        // If a fault occurs, notify with an error message.
        refreshManager.on(() => {
          vscode.window
            .showErrorMessage(
              'An error occurred while refreshing code intelligence data!',
              'Get Help',
            )
            .then((selection) => {
              if (selection === 'Get Help') {
                launchTroubleshootingLink(
                  'failed-to-refresh-code-intelligence',
                );
              }
            });
          resolve();
          return OK;
        }, 'fault');

        refreshManager.on(() => {
          resolve();
          return OK;
        }, 'idle');
      });
    },
  );
}
