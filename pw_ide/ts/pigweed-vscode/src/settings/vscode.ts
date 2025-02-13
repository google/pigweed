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

import logger from '../logging';

interface Setting<T> {
  (): T | undefined;
  (value: T | undefined): Thenable<void>;
}

type TerminalShell = 'bash' | 'zsh';

export interface Settings {
  activateBazeliskInNewTerminals: Setting<boolean>;
  codeAnalysisTarget: Setting<string>;
  codeAnalysisTargetDir: Setting<string>;
  disableBazelSettingsRecommendations: Setting<boolean>;
  disableBazeliskCheck: Setting<boolean>;
  disableCompileCommandsFileWatcher: Setting<boolean>;
  disableInactiveFileNotice: Setting<boolean>;
  disableInactiveFileCodeIntelligence: Setting<boolean>;
  enforceExtensionRecommendations: Setting<boolean>;
  hideInactiveFileIndicators: Setting<boolean>;
  preserveBazelPath: Setting<boolean>;
  projectRoot: Setting<string>;
  refreshCompileCommandsTarget: Setting<string>;
  terminalShell: Setting<TerminalShell>;
}

export type ConfigAccessor<T> = {
  get(): T | undefined;
  update(value: T | undefined): Thenable<void>;
};

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
export function stringSettingFor<T extends string = string>(
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
export function boolSettingFor(section: string, category = 'pigweed') {
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

function activateBazeliskInNewTerminals(): boolean;
function activateBazeliskInNewTerminals(
  value: boolean | undefined,
): Thenable<void>;
function activateBazeliskInNewTerminals(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('activateBazeliskInNewTerminals');
  if (value === undefined) return get() ?? false;
  return update(value);
}

function codeAnalysisTarget(): string | undefined;
function codeAnalysisTarget(value: string | undefined): Thenable<void>;
function codeAnalysisTarget(
  value?: string,
): string | undefined | Thenable<void> {
  const { get, update } = stringSettingFor('codeAnalysisTarget');
  if (value === undefined) return get();
  return update(value);
}

function codeAnalysisTargetDir(): string | undefined;
function codeAnalysisTargetDir(value: string | undefined): Thenable<void>;
function codeAnalysisTargetDir(
  value?: string,
): string | undefined | Thenable<void> {
  const { get, update } = stringSettingFor('codeAnalysisTargetDir');
  if (value === undefined) return get();
  return update(value);
}

function disableBazelSettingsRecommendations(): boolean;
function disableBazelSettingsRecommendations(
  value: boolean | undefined,
): Thenable<void>;
function disableBazelSettingsRecommendations(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('disableBazelSettingsRecommendations');
  if (value === undefined) return get() ?? false;
  return update(value);
}

function disableBazeliskCheck(): boolean;
function disableBazeliskCheck(value: boolean | undefined): Thenable<void>;
function disableBazeliskCheck(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('disableBazeliskCheck');
  if (value === undefined) return get() ?? false;
  return update(value);
}

function disableInactiveFileNotice(): boolean;
function disableInactiveFileNotice(value: boolean | undefined): Thenable<void>;
function disableInactiveFileNotice(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('disableInactiveFileNotice');
  if (value === undefined) return get() ?? false;
  return update(value);
}

function disableInactiveFileCodeIntelligence(): boolean;
function disableInactiveFileCodeIntelligence(
  value: boolean | undefined,
): Thenable<void>;
function disableInactiveFileCodeIntelligence(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('disableInactiveFileCodeIntelligence');
  if (value === undefined) return get() ?? true;
  return update(value);
}

function disableCompileCommandsFileWatcher(): boolean;
function disableCompileCommandsFileWatcher(
  value: boolean | undefined,
): Thenable<void>;
function disableCompileCommandsFileWatcher(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('disableCompileCommandsFileWatcher');
  if (value === undefined) return get() ?? false;
  return update(value);
}

function enforceExtensionRecommendations(): boolean;
function enforceExtensionRecommendations(
  value: boolean | undefined,
): Thenable<void>;
function enforceExtensionRecommendations(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('enforceExtensionRecommendations');
  if (value === undefined) return get() ?? false;
  return update(value);
}

function hideInactiveFileIndicators(): boolean;
function hideInactiveFileIndicators(value: boolean | undefined): Thenable<void>;
function hideInactiveFileIndicators(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('hideInactiveFileIndicators');
  if (value === undefined) return get() ?? false;
  update(value);
}

function preserveBazelPath(): boolean;
function preserveBazelPath(value: boolean | undefined): Thenable<void>;
function preserveBazelPath(
  value?: boolean,
): boolean | undefined | Thenable<void> {
  const { get, update } = boolSettingFor('preserveBazelPath');
  if (value === undefined) return get() ?? false;
  update(value);
}

function projectRoot(): string | undefined;
function projectRoot(value: string | undefined): Thenable<void>;
function projectRoot(value?: string): string | undefined | Thenable<void> {
  const { get, update } = stringSettingFor('projectRoot');
  if (value === undefined) return get();
  return update(value);
}

function refreshCompileCommandsTarget(): string;
function refreshCompileCommandsTarget(
  value: string | undefined,
): Thenable<void>;
function refreshCompileCommandsTarget(
  value?: string,
): string | undefined | Thenable<void> {
  const { get, update } = stringSettingFor('refreshCompileCommandsTarget');
  if (value === undefined) return get() ?? '//:refresh_compile_commands';
  return update(value);
}

function terminalShell(): TerminalShell;
function terminalShell(value: TerminalShell | undefined): Thenable<void>;
function terminalShell(
  value?: TerminalShell | undefined,
): TerminalShell | undefined | Thenable<void> {
  const { get, update } = stringSettingFor<TerminalShell>('terminalShell');
  if (value === undefined) return get() ?? 'bash';
  return update(value);
}

/** Entry point for accessing settings. */
export const settings: Settings = {
  activateBazeliskInNewTerminals,
  codeAnalysisTarget,
  codeAnalysisTargetDir,
  disableBazelSettingsRecommendations,
  disableBazeliskCheck,
  disableCompileCommandsFileWatcher,
  disableInactiveFileNotice,
  disableInactiveFileCodeIntelligence,
  enforceExtensionRecommendations,
  hideInactiveFileIndicators,
  preserveBazelPath,
  projectRoot,
  refreshCompileCommandsTarget,
  terminalShell,
};

// Config accessors for Bazel extension settings.
export const bazel_codelens = boolSettingFor('enableCodeLens', 'bazel');
export const bazel_executable = stringSettingFor('executable', 'bazel');
export const buildifier_executable = stringSettingFor(
  'buildifierExecutable',
  'bazel',
);

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

export interface WorkingDirStore {
  get(): string;
  set(path: string): void;
}

let workingDirStore: WorkingDirStoreImpl;

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
class WorkingDirStoreImpl implements WorkingDirStore {
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

export const workingDir = new WorkingDirStoreImpl(defaultWorkingDir());
