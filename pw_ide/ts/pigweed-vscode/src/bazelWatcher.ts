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

/**
 * A file watcher for Bazel files.
 *
 * We use this to automatically trigger compile commands refreshes on changes
 * to the build graph.
 */

import * as vscode from 'vscode';

import { spawn } from 'child_process';

import {
  RefreshCallback,
  OK,
  refreshManager,
  RefreshCallbackResult,
} from './refreshManager';
import logger from './logging';
import { getPigweedProjectRoot } from './project';
import { bazel_executable, settings, workingDir } from './settings';

/**
 * Create a container for a process running the refresh compile commands target.
 *
 * @return Refresh callbacks to do the refresh and to abort
 */
function createRefreshProcess(): [RefreshCallback, () => void] {
  // This provides us a handle to abort the process, if needed.
  const refreshController = new AbortController();
  const abort = () => refreshController.abort();
  const signal = refreshController.signal;

  // This callback will be registered with the RefreshManager to be called
  // when it's time to do the refresh.
  const cb: RefreshCallback = async () => {
    // This package is an ES module, but we're still building with CommonJS
    // modules. This is the workaround.
    // TODO: https://pwbug.dev/354034542 - Change when we're ES modules
    const { default: stripAnsi } = await import('strip-ansi');

    const cleanLogLine = (line: Buffer) => {
      // Remove ANSI escape codes that aren't supported in the output window
      const stripped = stripAnsi(line.toString());

      // Remove superfluous newlines
      if (stripped.at(-1) === '\n') {
        return stripped.substring(0, stripped.length - 1);
      }

      return stripped;
    };

    const cwd = (await getPigweedProjectRoot(settings, workingDir)) as string;

    logger.info('Refreshing compile commands');

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
    const spawnedProcess = spawn(cmd, args, { cwd, signal });

    spawnedProcess.on('spawn', () => {
      logger.info(`Running ${cmd} ${args.join(' ')}`);
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
    });

    spawnedProcess.on('error', (err) => {
      const { name, message } = err;

      if (name === 'ABORT_ERR') {
        logger.info('Aborted refreshing compile commands');
      } else {
        logger.error(`[${name}] while refreshing compile commands: ${message}`);
        result = { error: message };
      }
    });

    // All of the output actually goes out on stderr
    spawnedProcess.stderr.on('data', (data) => logger.info(cleanLogLine(data)));

    return result;
  };

  return [cb, abort];
}

/** Trigger a refresh compile commands process. */
export async function refreshCompileCommands() {
  const [cb, abort] = createRefreshProcess();

  const wrappedAbort = () => {
    abort();
    return OK;
  };

  refreshManager.onOnce(cb, 'refreshing');
  refreshManager.onOnce(wrappedAbort, 'abort');
  refreshManager.refresh();
}

/** Create file watchers to refresh compile commands on Bazel file changes. */
export function initRefreshCompileCommandsWatcher(): { dispose: () => void } {
  const watchers = [
    vscode.workspace.createFileSystemWatcher('**/BUILD.bazel'),
    vscode.workspace.createFileSystemWatcher('**/WORKSPACE'),
    vscode.workspace.createFileSystemWatcher('**/*.bzl'),
  ];

  watchers.forEach((watcher) => {
    watcher.onDidChange(refreshCompileCommands);
    watcher.onDidCreate(refreshCompileCommands);
    watcher.onDidDelete(refreshCompileCommands);
  });

  return {
    dispose: () => watchers.forEach((watcher) => watcher.dispose()),
  };
}
