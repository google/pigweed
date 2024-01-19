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
import logger from './logging';

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
  const shell = vscode.workspace
    .getConfiguration('pigweed')
    .get('terminalShell') as string;

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
