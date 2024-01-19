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

import { getExtensionsJson } from './config';
import { launchBootstrapTerminal, launchTerminal } from './terminal';

const bugUrl =
  'https://issues.pigweed.dev/issues/new?component=1194524&template=1911548';

/**
 * Open the bug report template in the user's browser.
 */
function fileBug() {
  vscode.env.openExternal(vscode.Uri.parse(bugUrl));
}

/**
 * Open the extensions sidebar and show the provided extensions.
 * @param extensions - A list of extension IDs
 */
function showExtensions(extensions: string[]) {
  vscode.commands.executeCommand(
    'workbench.extensions.search',
    '@id:' + extensions.join(', @id:'),
  );
}

/**
 * Given a list of extensions, return the subset that are not installed or are
 * disabled.
 * @param extensions - A list of extension IDs
 * @returns A list of extension IDs
 */
function getUnavailableExtensions(extensions: string[]): string[] {
  const unavailableExtensions: string[] = [];
  const available = vscode.extensions.all;

  // TODO(chadnorvell): Verify that this includes disabled extensions
  extensions.map(async (extId) => {
    const ext = available.find((ext) => ext.id == extId);

    if (!ext) {
      unavailableExtensions.push(extId);
    }
  });

  return unavailableExtensions;
}

/**
 * If there are recommended extensions that are not installed or enabled in the
 * current workspace, prompt the user to install them. This is "sticky" in the
 * sense that it will keep bugging the user to enable those extensions until
 * they enable them all, or until they explicitly cancel.
 * @param recs - A list of extension IDs
 */
async function installRecommendedExtensions(recs: string[]): Promise<void> {
  let unavailableRecs = getUnavailableExtensions(recs);
  const totalNumUnavailableRecs = unavailableRecs.length;
  let numUnavailableRecs = totalNumUnavailableRecs;

  const update = () => {
    unavailableRecs = getUnavailableExtensions(recs);
    numUnavailableRecs = unavailableRecs.length;
  };

  const wait = async () => new Promise((resolve) => setTimeout(resolve, 2500));

  const progressIncrement = (num: number) =>
    1 - (num / totalNumUnavailableRecs) * 100;

  // All recommendations are installed; we're done.
  if (totalNumUnavailableRecs == 0) {
    console.log('User has all recommended extensions');

    return;
  }

  showExtensions(unavailableRecs);

  vscode.window.showInformationMessage(
    `This Pigweed project needs you to install ${totalNumUnavailableRecs} ` +
      'required extensions. Install the extensions shown the extensions tab.',
    { modal: true },
    'Ok',
  );

  vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Notification,
      title:
        'Install these extensions! This Pigweed project needs these recommended extensions to be installed.',
      cancellable: true,
    },
    async (progress, token) => {
      while (numUnavailableRecs > 0) {
        // TODO(chadnorvell): Wait for vscode.extensions.onDidChange
        await wait();
        update();

        progress.report({
          increment: progressIncrement(numUnavailableRecs),
        });

        if (numUnavailableRecs > 0) {
          console.log(
            `User lacks ${numUnavailableRecs} recommended extensions`,
          );

          showExtensions(unavailableRecs);
        }

        if (token.isCancellationRequested) {
          console.log('User cancelled recommended extensions check');

          break;
        }
      }

      console.log('All recommended extensions are enabled');
      vscode.commands.executeCommand(
        'workbench.action.toggleSidebarVisibility',
      );
      progress.report({ increment: 100 });
    },
  );
}

/**
 * Given a list of extensions, return the subset that are enabled.
 * @param extensions - A list of extension IDs
 * @returns A list of extension IDs
 */
function getEnabledExtensions(extensions: string[]): string[] {
  const enabledExtensions: string[] = [];
  const available = vscode.extensions.all;

  // TODO(chadnorvell): Verify that this excludes disabled extensions
  extensions.map(async (extId) => {
    const ext = available.find((ext) => ext.id == extId);

    if (ext) {
      enabledExtensions.push(extId);
    }
  });

  return enabledExtensions;
}

/**
 * If there are unwanted extensions that are enabled in the current workspace,
 * prompt the user to disable them. This is "sticky" in the sense that it will
 * keep bugging the user to disable those extensions until they disable them
 * all, or until they explicitly cancel.
 * @param recs - A list of extension IDs
 */
async function disableUnwantedExtensions(unwanted: string[]) {
  let enabledUnwanted = getEnabledExtensions(unwanted);
  const totalNumEnabledUnwanted = enabledUnwanted.length;
  let numEnabledUnwanted = totalNumEnabledUnwanted;

  const update = () => {
    enabledUnwanted = getEnabledExtensions(unwanted);
    numEnabledUnwanted = enabledUnwanted.length;
  };

  const wait = async () => new Promise((resolve) => setTimeout(resolve, 2500));

  const progressIncrement = (num: number) =>
    1 - (num / totalNumEnabledUnwanted) * 100;

  // All unwanted are disabled; we're done.
  if (totalNumEnabledUnwanted == 0) {
    console.log('User has no unwanted extensions enabled');

    return;
  }

  showExtensions(enabledUnwanted);

  vscode.window.showInformationMessage(
    `This Pigweed project needs you to disable ${totalNumEnabledUnwanted} ` +
      'incompatible extensions. ' +
      'Disable the extensions shown the extensions tab.',
    { modal: true },
    'Ok',
  );

  vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Notification,
      title:
        'Disable these extensions! This Pigweed project needs these extensions to be disabled.',
      cancellable: true,
    },
    async (progress, token) => {
      while (numEnabledUnwanted > 0) {
        // TODO(chadnorvell): Wait for vscode.extensions.onDidChange
        await wait();
        update();

        progress.report({
          increment: progressIncrement(numEnabledUnwanted),
        });

        if (numEnabledUnwanted > 0) {
          console.log(
            `User has ${numEnabledUnwanted} unwanted extensions enabled`,
          );

          showExtensions(enabledUnwanted);
        }

        if (token.isCancellationRequested) {
          console.log('User cancelled unwanted extensions check');

          break;
        }
      }

      console.log('All unwanted extensions are disabled');
      vscode.commands.executeCommand(
        'workbench.action.toggleSidebarVisibility',
      );
      progress.report({ increment: 100 });
    },
  );
}

async function checkExtensions() {
  const extensions = await getExtensionsJson();

  const num_recommendations = extensions?.recommendations?.length ?? 0;
  const num_unwanted = extensions?.unwantedRecommendations?.length ?? 0;

  if (extensions && num_recommendations > 0) {
    await installRecommendedExtensions(extensions.recommendations as string[]);
  }

  if (extensions && num_unwanted > 0) {
    await disableUnwantedExtensions(
      extensions.unwantedRecommendations as string[],
    );
  }
}

function registerCommands(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.file-bug', () => fileBug()),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.check-extensions', () =>
      checkExtensions(),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.launch-terminal', () =>
      launchTerminal(),
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.bootstrap-terminal', () =>
      launchBootstrapTerminal(),
    ),
  );
}

export async function activate(context: vscode.ExtensionContext) {
  registerCommands(context);

  const shouldEnforce = vscode.workspace
    .getConfiguration('pigweed')
    .get('enforceExtensionRecommendations') as string;

  if (shouldEnforce === 'true') {
    console.log('pigweed.enforceExtensionRecommendations: true');
    await checkExtensions();
  }
}

export function deactivate() {
  // Do nothing.
}
