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

import * as child_process from 'child_process';

import * as vscode from 'vscode';

import { Disposable } from './disposables';
import logger from './logging';
import { getPigweedProjectRoot } from './project';

import {
  RefreshCallback,
  OK,
  RefreshManager,
  RefreshCallbackResult,
} from './refreshManager';

import { bazel_executable, settings, workingDir } from './settings';

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

/**
 * Create a container for a process running the refresh compile commands target.
 *
 * @return Refresh callbacks to do the refresh and to abort it
 */
function createRefreshProcess(): [RefreshCallback, () => void] {
  // This provides us a handle to abort the process, if needed.
  const refreshController = new AbortController();
  const abort = () => refreshController.abort();
  const signal = refreshController.signal;

  // This callback will be registered with the RefreshManager to be called
  // when it's time to do the refresh.
  const cb: RefreshCallback = async () => {
    logger.info('Refreshing compile commands');
    const cwd = (await getPigweedProjectRoot(settings, workingDir)) as string;
    const cmd = bazel_executable.get();

    if (!cmd) {
      const message = "Couldn't find a Bazel or Bazelisk executable";
      logger.error(message);
      return { error: message };
    }

    const refreshTarget = settings.refreshCompileCommandsTarget();

    if (!refreshTarget) {
      const message =
        "There's no configured Bazel target to refresh compile commands";
      logger.error(message);
      return { error: message };
    }

    const args = ['run', settings.refreshCompileCommandsTarget()!];
    let result: RefreshCallbackResult = OK;

    // TODO: https://pwbug.dev/350861417 - This should use the Bazel
    // extension commands instead, but doing that through the VS Code
    // command API is not simple.
    const spawnedProcess = child_process.spawn(cmd, args, { cwd, signal });

    // Wrapping this in a promise that only resolves on exit or error ensures
    // that this refresh callback blocks until the spawned process is complete.
    // Otherwise, the callback would return early while the spawned process is
    // still executing, prematurely moving on to later refresh manager states
    // that depend on *this* callback being finished.
    return new Promise((resolve) => {
      spawnedProcess.on('spawn', () => {
        logger.info(`Running ${cmd} ${args.join(' ')}`);
      });

      // All of the output actually goes out on stderr
      spawnedProcess.stderr.on('data', (data) =>
        logger.info(cleanLogLine(data)),
      );

      spawnedProcess.on('error', (err) => {
        const { name, message } = err;

        if (name === 'ABORT_ERR') {
          logger.info('Aborted refreshing compile commands');
        } else {
          logger.error(
            `[${name}] while refreshing compile commands: ${message}`,
          );
          result = { error: message };
        }

        resolve(result);
      });

      spawnedProcess.on('exit', (code) => {
        if (code === 0) {
          logger.info('Finished refreshing compile commands');
        } else {
          const message =
            'Failed to complete compile commands refresh ' +
            `(error code: ${code})`;

          logger.error(message);
          result = { error: message };
        }

        resolve(result);
      });
    });
  };

  return [cb, abort];
}

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
    const [cb, abort] = createRefreshProcess();

    const wrappedAbort = () => {
      abort();
      return OK;
    };

    this.refreshManager.onOnce(cb, 'refreshing');
    this.refreshManager.onOnce(wrappedAbort, 'abort');
    this.refreshManager.refresh();
  };
}
