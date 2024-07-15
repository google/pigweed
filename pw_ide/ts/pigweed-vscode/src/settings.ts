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

import logger from './logging';

interface Setting<T> {
  (): T | undefined;
  (value: T | undefined): void;
}

type ProjectType = 'bootstrap' | 'bazel';
type TerminalShell = 'bash' | 'zsh';

export interface Settings {
  enforceExtensionRecommendations: Setting<boolean>;
  projectRoot: Setting<string>;
  projectType: Setting<ProjectType>;
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

function projectRoot(): string | undefined;
function projectRoot(value: string | undefined): void;
function projectRoot(value?: string): string | undefined {
  const { get, update } = stringSettingFor('projectRoot');
  if (!value) return get();
  update(value);
}

function projectType(): ProjectType | undefined;
function projectType(value: ProjectType | undefined): void;
function projectType(value?: ProjectType | undefined): ProjectType | undefined {
  const { get, update } = stringSettingFor<ProjectType>('projectType');
  if (!value) return get();
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
  projectRoot,
  projectType,
  terminalShell,
};

/** Find the root directory of the project open in the editor. */
function editorRootDir(): vscode.WorkspaceFolder {
  const dirs = vscode.workspace.workspaceFolders;

  if (!dirs || dirs.length === 0) {
    logger.error(
      "Couldn't get editor root dir. There's no directory open in the editor!",
    );

    throw new Error("There's no directory open in the editor!");
  }

  if (dirs.length > 1) {
    logger.error(
      "Couldn't get editor root dir. " +
        "This is a multiroot workspace, which isn't currently supported.",
    );

    throw new Error(
      "This is a multiroot workspace, which isn't currently supported.",
    );
  }

  return dirs[0];
}

/** This should be used in place of, e.g., process.cwd(). */
const defaultWorkingDir = () => editorRootDir().uri.fsPath;

let workingDirStore: WorkingDirStore;

/**
 * A singleton for storing the project working directory.
 *
 * The location of this path could vary depending on project structure, and it
 * could be stored in settings, or it may need to be inferred by traversing the
 * project structure. The latter could be slow and shouldn't be repeated every
 * time we need something as basic as the project root.
 *
 * So compute the working dir path once, store it here, then fetch it whenever
 * you want without worrying about performance. The only downside is that you
 * need to make sure you set a value early in your execution path.
 *
 * This also serves as a platform-independent interface for the working dir
 * (for example, in Jest tests we don't have access to `vscode` so most of our
 * directory traversal strategies are unavailable).
 */
class WorkingDirStore {
  constructor(path?: string) {
    if (workingDirStore) {
      throw new Error("This is a singleton. You can't create it!");
    }

    if (path) {
      this._path = path;
    }

    // eslint-disable-next-line @typescript-eslint/no-this-alias
    workingDirStore = this;
  }

  _path: string | undefined = undefined;

  set(path: string) {
    this._path = path;
  }

  get(): string {
    if (!this._path) {
      throw new Error(
        'Yikes! You tried to get this value without setting it first.',
      );
    }

    return this._path;
  }
}

export const workingDir = new WorkingDirStore(defaultWorkingDir());
