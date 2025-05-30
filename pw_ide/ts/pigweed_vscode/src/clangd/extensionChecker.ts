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
import logging from '../logging';

// The minimum required Clangd version
const MIN_CLANGD_VERSION = '0.1.35';
const CLANGD_EXTENSION_ID = 'llvm-vs-code-extensions.vscode-clangd';

/**
 * Compares two semantic version strings (e.g., '1.2.3').
 * Returns:
 * 0 if versions are equal
 * < 0 if versionA is less than versionB
 * > 0 if versionA is greater than versionB
 */
function compareVersions(versionA: string, versionB: string): number {
  const partsA = versionA.split('.').map(Number);
  const partsB = versionB.split('.').map(Number);

  for (let i = 0; i < Math.max(partsA.length, partsB.length); i++) {
    const numA = partsA[i] || 0;
    const numB = partsB[i] || 0;

    if (numA < numB) {
      return -1;
    }
    if (numA > numB) {
      return 1;
    }
  }
  return 0; // Versions are equal
}

/**
 * Checks if the Clangd extension is installed and meets the minimum version.
 * If not, it shows a warning and provides options to retry or open the extension page.
 * @returns true if Clangd meets requirements, false otherwise.
 */
export async function checkClangdVersion(): Promise<boolean> {
  const clangdExtension = vscode.extensions.getExtension(CLANGD_EXTENSION_ID);

  if (!clangdExtension) {
    // Clangd extension not found
    const selection = await vscode.window.showErrorMessage(
      'Pigweed extension needs clangd 0.1.35 or greater to function. Switch to pre-release if you have clangd extension already installed.',
      'Install Clangd',
    );

    if (selection === 'Install Clangd') {
      await vscode.commands.executeCommand(
        'workbench.extensions.search',
        CLANGD_EXTENSION_ID,
      );
      // Show option to recheck
      const selection = await vscode.window.showInformationMessage(
        'Click Retry once you have installed the extension.',
        'Retry',
      );
      if (selection === 'Retry') {
        return await checkClangdVersion(); //
      }
    }
    return false;
  }

  const installedVersion = clangdExtension.packageJSON.version;

  if (compareVersions(installedVersion, MIN_CLANGD_VERSION) < 0) {
    // Clangd version is too old
    const selection = await vscode.window.showErrorMessage(
      `Pigweed extension needs clangd ${MIN_CLANGD_VERSION} or greater to function. Your current Clangd version is ${installedVersion}.`,
      'Update Clangd',
    );

    if (selection === 'Update Clangd') {
      await vscode.commands.executeCommand(
        'workbench.extensions.search',
        CLANGD_EXTENSION_ID,
      );
      const selection = await vscode.window.showInformationMessage(
        'Click Retry once you have installed the new extension.',
        'Retry',
      );
      if (selection === 'Retry') {
        return await checkClangdVersion();
      }
    }
    return false;
  }

  // Clangd is installed and meets the version requirement
  logging.info(
    `Clangd extension (ID: ${CLANGD_EXTENSION_ID}, Version: ${installedVersion}) meets requirements.`,
  );
  return true;
}
