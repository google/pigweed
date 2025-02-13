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
import { ExtensionContext } from 'vscode';

import {
  configureBazelisk,
  configureBazelSettings,
  interactivelySetBazeliskPath,
  setBazelRecommendedSettings,
  shouldSupportBazel,
} from './bazel';

import {
  BazelRefreshCompileCommandsWatcher,
  showProgressDuringRefresh,
} from './bazelWatcher';

import {
  ClangdActiveFilesCache,
  disableInactiveFileCodeIntelligence,
  enableInactiveFileCodeIntelligence,
  initBazelClangdPath,
  refreshCompileCommandsAndSetTarget,
  setCompileCommandsTarget,
  setCompileCommandsTargetOnSettingsChange,
} from './clangd';

import { getSettingsData, syncSettingsSharedToProject } from './configParsing';
import { Disposer } from './disposables';
import { didInit, linkRefreshManagerToEvents } from './events';
import { checkExtensions } from './extensionManagement';
import { InactiveFileDecorationProvider } from './inactiveFileDecoration';
import logger, { output } from './logging';
import { fileBug, launchTroubleshootingLink } from './links';

import {
  getPigweedProjectRoot,
  isBazelProject,
  isBootstrapProject,
} from './project';
import { RefreshManager } from './refreshManager';
import { settings, workingDir } from './settings/vscode';
import {
  ClangdFileWatcher,
  SettingsFileWatcher,
} from './settings/settingsWatcher';

import {
  InactiveVisibilityStatusBarItem,
  TargetStatusBarItem,
} from './statusBar';

import {
  launchBootstrapTerminal,
  launchTerminal,
  patchBazeliskIntoTerminalPath,
} from './terminal';

import { commandRegisterer, VscCommandCallback } from './utils';
import { shouldSupportGn } from './gn';
import { shouldSupportCmake } from './cmake';

interface CommandEntry {
  name: string;
  callback: VscCommandCallback;
  // Commands can be defined to be registered under certain project conditions:
  // - 'bazel': When it's only a Bazel project
  // - 'bootstrap': When it's only a bootstrap project
  // - 'both': When a project supports both Bazel and bootstrap
  // - 'any': The command should run under any circumstances
  // These can be combined, e.g., a command with ['bazel', 'both'] will be
  // registered in projects that are Bazel only and projects that use both
  // Bazel and bootstrap, but not projects that are bootstrap only.
  projectType: ('bazel' | 'bootstrap' | 'both' | 'any')[];
}

const disposer = new Disposer();

type ProjectType = 'bazel' | 'bootstrap' | 'both';

function registerCommands(
  projectType: ProjectType,
  context: ExtensionContext,
  refreshManager: RefreshManager<any>,
  clangdActiveFilesCache: ClangdActiveFilesCache,
  bazelCompileCommandsWatcher?: BazelRefreshCompileCommandsWatcher | undefined,
): void {
  const commands: CommandEntry[] = [
    {
      name: 'pigweed.open-output-panel',
      callback: output.show,
      projectType: ['any'],
    },
    {
      name: 'pigweed.file-bug',
      callback: fileBug,
      projectType: ['any'],
    },
    {
      name: 'pigweed.check-extensions',
      callback: checkExtensions,
      projectType: ['any'],
    },
    {
      name: 'pigweed.sync-settings',
      callback: async () =>
        syncSettingsSharedToProject(await getSettingsData(), true),
      projectType: ['any'],
    },
    {
      name: 'pigweed.disable-inactive-file-code-intelligence',
      callback: () =>
        disableInactiveFileCodeIntelligence(clangdActiveFilesCache),
      projectType: ['any'],
    },

    {
      name: 'pigweed.enable-inactive-file-code-intelligence',
      callback: () =>
        enableInactiveFileCodeIntelligence(clangdActiveFilesCache),
      projectType: ['any'],
    },

    {
      name: 'pigweed.select-target',
      callback: () => setCompileCommandsTarget(clangdActiveFilesCache),
      projectType: ['any'],
    },
    {
      name: 'pigweed.launch-terminal',
      callback: launchTerminal,
      projectType: ['bootstrap', 'both'],
    },
    {
      name: 'pigweed.launch-terminal',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bazel projects',
        ),
      projectType: ['bazel'],
    },
    {
      name: 'pigweed.bootstrap-terminal',
      callback: launchBootstrapTerminal,
      projectType: ['bootstrap', 'both'],
    },
    {
      name: 'pigweed.bootstrap-terminal',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bazel projects',
        ),
      projectType: ['bazel'],
    },
    {
      name: 'pigweed.set-bazel-recommended-settings',
      callback: setBazelRecommendedSettings,
      projectType: ['bazel', 'both'],
    },
    {
      name: 'pigweed.set-bazel-recommended-settings',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
      projectType: ['bootstrap'],
    },
    {
      name: 'pigweed.set-bazelisk-path',
      callback: interactivelySetBazeliskPath,
      projectType: ['bazel', 'both'],
    },
    {
      name: 'pigweed.set-bazelisk-path',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
      projectType: ['bootstrap'],
    },
    {
      name: 'pigweed.activate-bazelisk-in-terminal',
      callback: patchBazeliskIntoTerminalPath,
      projectType: ['bazel', 'both'],
    },
    {
      name: 'pigweed.activate-bazelisk-in-terminal',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
      projectType: ['bootstrap'],
    },
    {
      name: 'pigweed.refresh-compile-commands',
      callback: () => {
        bazelCompileCommandsWatcher!.refresh();
        showProgressDuringRefresh(refreshManager);
      },
      projectType: ['bazel', 'both'],
    },
    {
      name: 'pigweed.refresh-compile-commands',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
      projectType: ['bootstrap'],
    },
    {
      name: 'pigweed.refresh-compile-commands-and-set-target',
      callback: () => {
        refreshCompileCommandsAndSetTarget(
          bazelCompileCommandsWatcher!.refresh,
          refreshManager,
          clangdActiveFilesCache,
        );
      },
      projectType: ['bazel', 'both'],
    },
    {
      name: 'pigweed.refresh-compile-commands-and-set-target',
      callback: () =>
        vscode.window.showWarningMessage(
          'This command is currently not supported with Bootstrap projects',
        ),
      projectType: ['bootstrap'],
    },
  ];

  const commandsToRegister = commands.filter((c) => {
    if (c.projectType.includes('any')) return true;
    return c.projectType.includes(projectType);
  });

  const registerCommand = commandRegisterer(context);
  commandsToRegister.forEach((c) => registerCommand(c.name, c.callback));
}

async function initAsBazelProject(
  refreshManager: RefreshManager<any>,
  compileCommandsWatcher: BazelRefreshCompileCommandsWatcher,
) {
  // Do stuff that we want to do on load.
  await initBazelClangdPath();
  await configureBazelSettings();
  await configureBazelisk();
  linkRefreshManagerToEvents(refreshManager);

  if (settings.activateBazeliskInNewTerminals()) {
    vscode.window.onDidOpenTerminal(
      patchBazeliskIntoTerminalPath,
      disposer.disposables,
    );
  }

  if (!settings.disableCompileCommandsFileWatcher()) {
    compileCommandsWatcher.refresh();
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
async function configureProject(
  context: vscode.ExtensionContext,
  refreshManager: RefreshManager<any>,
  clangdActiveFilesCache: ClangdActiveFilesCache,
  forceProjectType?: 'bazel' | 'bootstrap' | undefined,
): Promise<ProjectType | undefined> {
  const projectTypes: ProjectType[] = [];

  // If we're missing a piece of information, we can ask the user to manually
  // provide it. If they do, we should re-run this flow, and that intent is
  // signaled by setting this var.
  let tryAgain = false;
  let tryAgainProjectType: 'bazel' | 'bootstrap' | undefined;

  const projectRoot = await getPigweedProjectRoot(settings, workingDir);

  if (projectRoot) {
    output.appendLine(`The Pigweed project root is ${projectRoot}`);
    let foundProjectBuildFile = false;

    if (isBazelProject(projectRoot) || forceProjectType === 'bazel') {
      output.appendLine('This is a Bazel project');
      projectTypes.push('bazel');
      foundProjectBuildFile = true;
    }

    if (isBootstrapProject(projectRoot) || forceProjectType === 'bootstrap') {
      output.appendLine(
        `This is ${foundProjectBuildFile ? 'also ' : ' '}bootstrap project`,
      );

      projectTypes.push('bootstrap');
      foundProjectBuildFile = true;
    }

    if (!foundProjectBuildFile) {
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
              tryAgainProjectType = 'bazel';
              vscode.window.showInformationMessage(
                'Configured as a Pigweed Bazel project',
              );
              tryAgain = true;
              break;
            }
            case 'Bootstrap': {
              tryAgainProjectType = 'bootstrap';
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
    await configureProject(
      context,
      refreshManager,
      clangdActiveFilesCache,
      tryAgainProjectType,
    );
  }

  return projectTypes.length === 2
    ? 'both'
    : projectTypes.length === 1
      ? projectTypes[0]
      : undefined;
}

function buildSystemStatusReason(
  settingsValue: boolean | 'auto',
  activeState: boolean,
): string {
  switch (settingsValue) {
    case true:
      return 'Enabled in settings.';
    case false:
      return 'Disabled in settings.';
    case 'auto':
      return `Build files ${activeState ? '' : "don't "}exist in project.`;
  }
}

export async function activate(context: vscode.ExtensionContext) {
  logger.info('Extension loaded');
  logger.info('');

  const useBazel = await shouldSupportBazel();
  const useCmake = await shouldSupportCmake();
  const useGn = await shouldSupportGn();

  logger.info(`Bazel support is ${useBazel ? 'ACTIVE' : 'DISABLED'}`);
  logger.info(
    `↳ Reason: ${buildSystemStatusReason(
      settings.supportBazelTargets(),
      useBazel,
    )}`,
  );
  logger.info('');

  logger.info(`CMake support is ${useCmake ? 'ACTIVE' : 'DISABLED'}`);
  logger.info(
    `↳ Reason: ${buildSystemStatusReason(
      settings.supportCmakeTargets(),
      useCmake,
    )}`,
  );
  logger.info('');

  logger.info(`GN support is ${useGn ? 'ACTIVE' : 'DISABLED'}`);
  logger.info(
    `↳ Reason: ${buildSystemStatusReason(settings.supportGnTargets(), useGn)}`,
  );
  logger.info('');

  // Marshall all of our components and dependencies.
  const refreshManager = disposer.add(RefreshManager.create({ logger }));

  const { clangdActiveFilesCache } = disposer.addMany({
    clangdActiveFilesCache: new ClangdActiveFilesCache(refreshManager),
    inactiveVisibilityStatusBarItem: new InactiveVisibilityStatusBarItem(),
    settingsFileWatcher: new SettingsFileWatcher(),
    targetStatusBarItem: new TargetStatusBarItem(),
  });

  disposer.add(new ClangdFileWatcher(clangdActiveFilesCache));
  disposer.add(new InactiveFileDecorationProvider(clangdActiveFilesCache));

  // Determine the project type and configuration parameters.
  const projectType = await configureProject(
    context,
    refreshManager,
    clangdActiveFilesCache,
  );

  if (projectType === undefined) {
    vscode.window
      .showErrorMessage(
        'A fatal error occurred while initializing the project. ' +
          'This was unexpected. The Pigweed extension will not function. ' +
          'Please file a bug with details about your project structure.',
        'File Bug',
        'Dismiss',
      )
      .then((selection) => {
        if (selection === 'File Bug') fileBug();
      });

    return;
  }

  if (projectType === 'bazel' || projectType === 'both') {
    const compileCommandsWatcher = new BazelRefreshCompileCommandsWatcher(
      refreshManager,
      settings.disableCompileCommandsFileWatcher(),
    );

    // Don't do Bazel init if the user has explicitly disabled Bazel support.
    if (useBazel) {
      await initAsBazelProject(refreshManager, compileCommandsWatcher);
    }

    registerCommands(
      projectType,
      context,
      refreshManager,
      clangdActiveFilesCache,
      compileCommandsWatcher,
    );
  } else {
    registerCommands(
      projectType,
      context,
      refreshManager,
      clangdActiveFilesCache,
    );
  }

  // If the current target is changed directly via a settings file change (in
  // other words, not by running a command), detect that and do all the other
  // stuff that the command would otherwise have done.
  vscode.workspace.onDidChangeConfiguration(
    setCompileCommandsTargetOnSettingsChange(clangdActiveFilesCache),
    disposer.disposables,
  );

  if (settings.enforceExtensionRecommendations()) {
    logger.info('Project is configured to enforce extension recommendations');
    await checkExtensions();
  }

  logger.info('Extension initialization complete');
  didInit.fire();
}

export function deactivate() {
  disposer.dispose();
}
