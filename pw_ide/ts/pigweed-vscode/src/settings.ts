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

interface Setting<T> {
  (): T | undefined;
  (value: T | undefined): void;
}

type TerminalShell = 'bash' | 'zsh';

export interface Settings {
  enforceExtensionRecommendations: Setting<boolean>;
  terminalShell: Setting<TerminalShell>;
}

/** Wrap the verbose ceremony of accessing/updating a particular setting. */
export function settingFor<T>(section: string, category = 'pigweed') {
  return {
    get: () =>
      vscode.workspace.getConfiguration(category).get(section) as T | undefined,
    update: (value: T | undefined) =>
      vscode.workspace.getConfiguration(category).update(section, value),
  };
}

/**
 * Wrap the verbose ceremony of accessing/updating a particular setting.
 *
 * This variation handles some edge cases of string settings, and allows you
 * to constrain the type of the string, e.g., to a union of literals.
 */
function stringSettingFor<T extends string = string>(
  section: string,
  category = 'pigweed',
) {
  return {
    get: (): T | undefined => {
      const current = vscode.workspace
        .getConfiguration(category)
        .get(section) as T | undefined;

      // Undefined settings can manifest as empty strings.
      if (current === undefined || current.length === 0) {
        return undefined;
      }

      return current;
    },
    update: (value: T | undefined): Thenable<void> =>
      vscode.workspace.getConfiguration(category).update(section, value),
  };
}

/**
 * Wrap the verbose ceremony of accessing/updating a particular setting.
 *
 * This variation handles some edge cases of boolean settings.
 */
function boolSettingFor(section: string, category = 'pigweed') {
  return {
    get: (): boolean | undefined => {
      const current = vscode.workspace
        .getConfiguration(category)
        .get(section) as boolean | string | undefined;

      // This seems obvious, but thanks to the edge cases handled below, we
      // need to compare actual values, not just truthiness.
      if (current === true) return true;
      if (current === false) return false;

      // Undefined settings can manifest as empty strings.
      if (current === undefined || current.length === 0) {
        return undefined;
      }

      // In some cases, booleans are returned as strings.
      if (current === 'true') return true;
      if (current === 'false') return false;
    },

    update: (value: boolean | undefined): Thenable<void> =>
      vscode.workspace.getConfiguration(category).update(section, value),
  };
}

function enforceExtensionRecommendations(): boolean;
function enforceExtensionRecommendations(value: boolean | undefined): void;
function enforceExtensionRecommendations(value?: boolean): boolean | undefined {
  const { get, update } = boolSettingFor('enforceExtensionRecommendations');
  if (!value) return get() ?? false;
  update(value);
}

function terminalShell(): TerminalShell;
function terminalShell(value: TerminalShell | undefined): void;
function terminalShell(
  value?: TerminalShell | undefined,
): TerminalShell | undefined {
  const { get, update } = stringSettingFor<TerminalShell>('terminalShell');
  if (!value) return get() ?? 'bash';
  update(value);
}

/** Entry point for accessing settings. */
export const settings: Settings = {
  enforceExtensionRecommendations,
  terminalShell,
};
