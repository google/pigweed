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

import {
  onSetCompileCommands,
  refreshCompileCommandsAndSetTarget,
  setCompileCommandsTarget,
  writeClangdSettingsFile,
} from './clangd';
import { checkExtensions } from './extensionManagement';
import logger, { output } from './logging';
import { fileBug, launchTroubleshootingLink } from './links';
import { settings, workingDir } from './settings';
import {
  initRefreshCompileCommandsWatcher,
  refreshCompileCommands,
} from './bazelWatcher';
import {
  getPigweedProjectRoot,
  isBazelWorkspaceProject,
  isBootstrapProject,
} from './project';
import { refreshManager } from './refreshManager';
import {
  launchBootstrapTerminal,
  launchTerminal,
  patchBazeliskIntoTerminalPath,
} from './terminal';
import {
  interactivelySetBazeliskPath,
  configureBazelisk,
  configureOtherBazelSettings,
  setBazelRecommendedSettings,
} from './bazel';
import {
  getInactiveVisibilityStatusBarItem,
  getTargetStatusBarItem,
  updateInactiveVisibilityStatusBarItem,
  updateTargetStatusBarItem,
} from './statusBar';
import {
  initClangdFileWatcher,
  initSettingsFilesWatcher,
} from './settingsWatcher';
import { getSettingsData, syncSettingsSharedToProject } from './configParsing';

// Anything that needs to be disposed of should be stored here.
const disposables: { dispose: () => void }[] = [output, refreshManager];

function registerUniversalCommands(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.open-output-panel', output.show),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.file-bug', fileBug),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.check-extensions',
      checkExtensions,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.sync-settings', async () =>
      syncSettingsSharedToProject(await getSettingsData(), true),
    ),
  );
}

function registerBootstrapCommands(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.launch-terminal', launchTerminal),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.bootstrap-terminal',
      launchBootstrapTerminal,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.disable-inactive-file-code-intelligence',
      () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.enable-inactive-file-code-intelligence',
      () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.refresh-compile-commands', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bootstrap projects',
      ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.refresh-compile-commands-and-set-target',
      () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.set-bazelisk-path', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bootstrap projects',
      ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.set-bazel-recommended-settings',
      () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.select-target', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bootstrap projects',
      ),
    ),
  );
}

async function registerBazelCommands(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.disable-inactive-file-code-intelligence',
      async () => {
        logger.info('Disabling inactive file code intelligence');
        await settings.disableInactiveFileCodeIntelligence(true);
        updateInactiveVisibilityStatusBarItem();
        await writeClangdSettingsFile(settings.codeAnalysisTarget());
        await vscode.commands.executeCommand('clangd.restart');
      },
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.enable-inactive-file-code-intelligence',
      async () => {
        logger.info('Enabling inactive file code intelligence');
        await settings.disableInactiveFileCodeIntelligence(false);
        updateInactiveVisibilityStatusBarItem();
        await writeClangdSettingsFile();
        await vscode.commands.executeCommand('clangd.restart');
      },
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.refresh-compile-commands',
      refreshCompileCommands,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.refresh-compile-commands-and-set-target',
      refreshCompileCommandsAndSetTarget,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.set-bazelisk-path',
      interactivelySetBazeliskPath,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.set-bazel-recommended-settings',
      setBazelRecommendedSettings,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.select-target',
      setCompileCommandsTarget,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.launch-terminal', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bazel projects',
      ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.bootstrap-terminal', () =>
      vscode.window.showWarningMessage(
        'This command is currently not supported with Bazel projects',
      ),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.activate-bazelisk-in-terminal',
      patchBazeliskIntoTerminalPath,
    ),
  );

  disposables.push(initSettingsFilesWatcher());
  disposables.push(initClangdFileWatcher());

  context.subscriptions.push(getTargetStatusBarItem());
  onSetCompileCommands(updateTargetStatusBarItem);
  refreshManager.on(updateTargetStatusBarItem, 'idle');
  refreshManager.on(updateTargetStatusBarItem, 'willRefresh');
  refreshManager.on(updateTargetStatusBarItem, 'refreshing');
  refreshManager.on(updateTargetStatusBarItem, 'didRefresh');
  refreshManager.on(updateTargetStatusBarItem, 'abort');
  refreshManager.on(updateTargetStatusBarItem, 'fault');

  context.subscriptions.push(getInactiveVisibilityStatusBarItem());

  if (!settings.disableCompileCommandsFileWatcher()) {
    await vscode.commands.executeCommand('pigweed.refresh-compile-commands');
    disposables.push(initRefreshCompileCommandsWatcher());
  }
}

/**
 * Get the project type and configuration details on startup.
 *
 * This is a little long and over-complex, but essentially it does just a few
 * things:
 *   - Do I know where the Pigweed project root is?
 *   - Do I know if this is a Bazel or bootstrap project?
 *   - If I don't know any of that, ask the user to tell me and save the
 *     selection to settings.
 *   - If the user needs help, route them to the right place.
 */
async function configureProject(context: vscode.ExtensionContext) {
  // If we're missing a piece of information, we can ask the user to manually
  // provide it. If they do, we should re-run this flow, and that intent is
  // signaled by setting this var.
  let tryAgain = false;

  const projectRoot = await getPigweedProjectRoot(settings, workingDir);

  if (projectRoot) {
    output.appendLine(`The Pigweed project root is ${projectRoot}`);

    if (
      settings.projectType() === 'bootstrap' ||
      isBootstrapProject(projectRoot)
    ) {
      output.appendLine('This is a bootstrap project');
      registerBootstrapCommands(context);
    } else if (
      settings.projectType() === 'bazel' ||
      isBazelWorkspaceProject(projectRoot)
    ) {
      output.appendLine('This is a Bazel project');
      await registerBazelCommands(context);
      await configureOtherBazelSettings();
      await configureBazelisk();
    } else {
      vscode.window
        .showErrorMessage(
          "I couldn't automatically determine what type of Pigweed project " +
            'this is. Choose one of these project types, or get more help.',
          'Bazel',
          'Bootstrap',
          'Get Help',
        )
        .then((selection) => {
          switch (selection) {
            case 'Bazel': {
              settings.projectType('bazel');
              vscode.window.showInformationMessage(
                'Configured as a Pigweed Bazel project',
              );
              tryAgain = true;
              break;
            }
            case 'Bootstrap': {
              settings.projectType('bootstrap');
              vscode.window.showInformationMessage(
                'Configured as a Pigweed Bootstrap project',
              );
              tryAgain = true;
              break;
            }
            case 'Get Help': {
              launchTroubleshootingLink('project-type');
              break;
            }
          }
        });
    }
  } else {
    vscode.window
      .showErrorMessage(
        "I couldn't automatically determine the location of the  Pigweed " +
          'root directory. Enter it manually, or get more help.',
        'Browse',
        'Get Help',
      )
      .then((selection) => {
        switch (selection) {
          case 'Browse': {
            vscode.window
              .showOpenDialog({
                canSelectFiles: false,
                canSelectFolders: true,
                canSelectMany: false,
              })
              .then((result) => {
                // The user can cancel, making result undefined
                if (result) {
                  const [uri] = result;
                  settings.projectRoot(uri.fsPath);
                  vscode.window.showInformationMessage(
                    `Set the Pigweed project root to: ${uri.fsPath}`,
                  );
                  tryAgain = true;
                }
              });
            break;
          }
          case 'Get Help': {
            launchTroubleshootingLink('project-root');
            break;
          }
        }
      });
  }

  // This should only re-run if something has materially changed, e.g., the user
  // provided a piece of information we needed.
  if (tryAgain) {
    await configureProject(context);
  }
}

export async function activate(context: vscode.ExtensionContext) {
  // Register commands that apply to all project types
  registerUniversalCommands(context);
  logger.info('Extension loaded');

  // Determine the project type and configuration parameters. This also
  // registers the commands specific to each project type.
  await configureProject(context);

  if (settings.enforceExtensionRecommendations()) {
    logger.info('Project is configured to enforce extension recommendations');
    await checkExtensions();
  }
}

export function deactivate() {
  disposables.forEach((item) => item.dispose());
}
