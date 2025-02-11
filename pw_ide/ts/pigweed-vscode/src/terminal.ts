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
import * as os from 'os';
import { randomBytes } from 'crypto';

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

  try {
    const { stdout } = await exec(`ps -p ${pid} -o comm=`);
    const processName = stdout.trim();

    if (processName) {
      logger.info(`Found shell process: ${processName}`);
      return path.basename(processName);
    }
  } catch (error) {
    logger.info(`Could not get direct process info: ${error}`);
  }

  try {
    let cmd: string;

    switch (process.platform) {
      case 'linux': {
        try {
          await exec('which pgrep');
          cmd = `pgrep -P ${pid} | xargs -r ps -o comm= -p`;
        } catch {
          cmd = `ps -e -o ppid=,comm= | grep "^[[:space:]]*${pid}"`;
        }
        break;
      }
      case 'darwin': {
        cmd = `ps -A -o ppid=,pid=,comm= | grep "^[[:space:]]*${pid}"`;
        break;
      }
      default: {
        logger.error(`Platform not currently supported: ${process.platform}`);
        return;
      }
    }

    const { stdout } = await exec(cmd);

    const processes = stdout
      .split('\n')
      .map((line) => line.trim())
      .filter((line) => line.length > 0);

    // Find the shell process by pid and extract the process name
    const shellProcesses = processes.filter((proc) =>
      /(?:sh|dash|bash|ksh|ash|zsh|fish)$/.test(proc),
    );

    if (shellProcesses.length > 0) {
      const shellName = path.basename(
        shellProcesses[0].split(/\s+/).pop() || '',
      );
      logger.info(`Found shell process: ${shellName}`);
      return shellName;
    }

    logger.error(`Could not find process with pid=${pid}`);
    logger.info(`Process tree: ${processes.join(', ')}`);
  } catch (error) {
    logger.error(`Failed to inspect process tree: ${error}`);
  }
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
    case 'fish': {
      cmd = `set -x --prepend PATH "${path.dirname(bazeliskPath)}"`;
      break;
    }
    default: {
      cmd = `export PATH="${path.dirname(bazeliskPath)}:$\{PATH}"`;
      break;
    }
  }

  logger.info(`Patching Bazelisk path into ${shellType} terminal`);
  terminal.sendText(cmd, true);
  terminal.show();
}

/**
 * This method runs the given commands in the active terminal
 * and returns the output text. Currently, the API does not
 * support reading terminal buffer so instead, we pipe output
 * to a temp text file and then read it out once execution
 * has completed.
 */
export async function executeInTerminalAndGetStdout(
  cmd: string,
): Promise<string> {
  const terminal = vscode.window.activeTerminal;

  if (!terminal) {
    throw new Error('No active terminal found.');
  }

  const tmpDir = os.tmpdir();
  const randomOutputFileName = `vscode-terminal-output-${randomBytes(
    8,
  ).toString('hex')}.txt`;
  const randomDoneFileName = `vscode-terminal-done-${randomBytes(8).toString(
    'hex',
  )}.txt`;
  const tmpOutputFilePath = path.join(tmpDir, randomOutputFileName);
  const tmpDoneFilePath = path.join(tmpDir, randomDoneFileName);

  try {
    // Construct the command to redirect output to the temp file and then touch the done file
    const commandToExecute = `${cmd} &> ${tmpOutputFilePath} && touch ${tmpDoneFilePath}\n`;

    terminal.sendText(commandToExecute);

    // Wait for the done file to exist
    while (!fs.existsSync(tmpDoneFilePath)) {
      await new Promise((resolve) => setTimeout(resolve, 10));
    }

    const output = fs.readFileSync(tmpOutputFilePath, 'utf-8');
    return output;
  } catch (error) {
    console.error('Error during command execution:', error);
    throw error;
  } finally {
    // Delete the temporary files
    fs.unlinkSync(tmpOutputFilePath);
    fs.unlinkSync(tmpDoneFilePath);
  }
}
