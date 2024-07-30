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

import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';

// Convert `exec` from callback style to promise style.
import { exec as cbExec } from 'child_process';
import util from 'node:util';
const exec = util.promisify(cbExec);

import * as vscode from 'vscode';

import { vendoredBazeliskPath } from './bazel';
import logger from './logging';
import { bazel_executable, settings } from './settings';

type InitScript = 'activate' | 'bootstrap';

/**
 * Generate the configuration to launch an activated or bootstrapped terminal.
 *
 * @param initScript The name of the script to be sourced on launch
 * @returns Options to be provided to terminal launch functions
 */
function getShellConfig(
  initScript: InitScript = 'activate',
): vscode.TerminalOptions {
  const shell = settings.terminalShell();

  return {
    name: 'Pigweed Terminal',
    shellPath: shell,
    shellArgs: ['-c', `. ./${initScript}.sh; exec ${shell} -i`],
  };
}

/**
 * Launch an activated terminal.
 */
export function launchTerminal() {
  const shellConfig = getShellConfig();
  logger.info(`Launching activated terminal with: ${shellConfig.shellPath}`);
  vscode.window.createTerminal(shellConfig).show();
}

/**
 * Launch a activated terminal by bootstrapping it.
 */
export function launchBootstrapTerminal() {
  const shellConfig = getShellConfig();
  logger.info(`Launching bootstrapepd terminal with: ${shellConfig.shellPath}`);
  vscode.window.createTerminal(getShellConfig('bootstrap')).show();
}

/**
 * Get the type of shell running in an integrated terminal, e.g. bash, zsh, etc.
 *
 * This is a bit roundabout; it grabs the shell pid because that seems to be
 * the only useful piece of information we can get from the VSC API. Then we
 * use `ps` to find the name of the binary associated with that pid.
 */
async function getShellTypeFromTerminal(
  terminal: vscode.Terminal,
): Promise<string | undefined> {
  const pid = await terminal.processId;

  if (!pid) {
    logger.error('Terminal has no pid');
    return;
  }

  logger.info(`Searching for shell with pid=${pid}`);

  let cmd: string;
  let pidPos: number;
  let namePos: number;

  switch (process.platform) {
    case 'linux': {
      cmd = `ps -A`;
      pidPos = 1;
      namePos = 4;
      break;
    }
    case 'darwin': {
      cmd = `ps -ax`;
      pidPos = 0;
      namePos = 3;
      break;
    }
    default: {
      logger.error(`Platform not currently supported: ${process.platform}`);
      return;
    }
  }

  const { stdout } = await exec(cmd);

  // Split the output into a list of processes, each of which is a tuple of
  // data from each column of the process table.
  const processes = stdout.split('\n').map((line) => line.split(/[ ]+/));

  // Find the shell process by pid and extract the process name
  const shellProcessName = processes
    .filter((it) => pid === parseInt(it[pidPos]))
    .at(0)
    ?.at(namePos);

  if (!shellProcessName) {
    logger.error(`Could not find process with pid=${pid}`);
    return;
  }

  return path.basename(shellProcessName);
}

/** Prepend the path to Bazelisk into the active terminal's path. */
export async function patchBazeliskIntoTerminalPath(
  terminal?: vscode.Terminal,
): Promise<void> {
  const bazeliskPath = bazel_executable.get() ?? vendoredBazeliskPath();

  if (!bazeliskPath) {
    logger.error(
      "Couldn't activate Bazelisk in terminal because none could be found",
    );
    return;
  }

  // When using the vendored Bazelisk, the binary name won't be `bazelisk` --
  // it will be something like `bazelisk-darwin_arm64`. But the user expects
  // to just run `bazelisk` in the terminal. So while this is not entirely
  // ideal, we just create a symlink in the same directory if the binary name
  // isn't plain `bazelisk`.
  if (path.basename(bazeliskPath) !== 'bazelisk') {
    try {
      fs.symlink(
        bazeliskPath,
        path.join(path.dirname(bazeliskPath), 'bazelisk'),
        (error) => {
          const message = error
            ? `${error.errno} ${error.message}`
            : 'unknown error';
          throw new Error(message);
        },
      );
    } catch (error: unknown) {
      logger.error(`Failed to create Bazelisk symlink for ${bazeliskPath}`);
      return;
    }
  }

  // Should grab the currently active terminal or most recently active, if a
  // specific terminal reference wasn't provided.
  terminal = terminal ?? vscode.window.activeTerminal;

  // If there's no terminal, create one
  if (!terminal) {
    terminal = vscode.window.createTerminal();
  }

  if (!terminal) {
    logger.error(
      "Couldn't activate Bazelisk in terminal because no terminals could be found",
    );
    return;
  }

  const shellType = await getShellTypeFromTerminal(terminal);

  let cmd: string;

  switch (shellType) {
    case 'bash':
    case 'zsh': {
      cmd = `export PATH="${path.dirname(bazeliskPath)}:$\{PATH}"`;
      break;
    }
    case 'fish': {
      cmd = `set -x --prepend PATH "${path.dirname(bazeliskPath)}"`;
      break;
    }
    default: {
      const message = shellType
        ? `This shell is not currently supported: ${shellType}`
        : "Couldn't determine how to activate Bazelisk in your terminal. " +
          'Check the Pigweed output panel for more information.';

      vscode.window.showErrorMessage(message);
      return;
    }
  }

  logger.info(`Patching Bazelisk path into ${shellType} terminal`);
  terminal.sendText(cmd, true);
  terminal.show();
}
