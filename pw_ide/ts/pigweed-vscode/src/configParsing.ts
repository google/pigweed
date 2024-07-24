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

import * as hjson from 'hjson';
import * as vscode from 'vscode';
import logger from './logging';

/**
 * Schema for settings.json
 */
export type SettingsJson = Record<string, any>;

/**
 * Schema for extensions.json
 */
export interface ExtensionsJson {
  recommendations?: string[];
  unwantedRecommendations?: string[];
}

/**
 * Partial schema for the workspace config file
 */
interface WorkspaceConfig {
  extensions?: ExtensionsJson;
  settings?: SettingsJson;
}

// When the project is opened directly (i.e., by opening the repo directory),
// we have direct access to extensions.json. But if the project is part of a
// workspace (https://code.visualstudio.com/docs/editor/workspaces), we'll get
// a combined config that includes the equivalent of extensions.json associated
// with the "extensions" key. This is taken into consideration only for the sake
// of completeness; Pigweed doesn't currently support the use of workspaces.
type LoadableExtensionsConfig = ExtensionsJson & WorkspaceConfig;

/**
 * Load a config file that contains extensions.json data. This could be
 * extensions.json itself, or a workspace file that contains the equivalent.
 * @param uri - A file path to load
 * @returns - The extensions.json file data
 */
export async function loadExtensionsJson(
  uri: vscode.Uri,
): Promise<ExtensionsJson> {
  const buffer = await vscode.workspace.fs.readFile(uri);
  const config: LoadableExtensionsConfig = hjson.parse(buffer.toString());

  if (config.extensions) {
    return config.extensions;
  }

  return config as ExtensionsJson;
}

/**
 * Find and return the extensions.json data for the project.
 * @param includeWorkspace - Also search workspace files
 * @returns The extensions.json file data
 */
export async function getExtensionsJson(
  includeWorkspace = false,
): Promise<ExtensionsJson | null> {
  const files = await vscode.workspace.findFiles(
    '.vscode/extensions.json',
    '**/node_modules/**',
  );

  if (includeWorkspace) {
    const workspaceFile = vscode.workspace.workspaceFile;

    if (workspaceFile) {
      files.push(workspaceFile);
    }
  }

  if (files.length == 0) {
    return null;
  } else {
    if (files.length > 1) {
      vscode.window.showWarningMessage(
        'Found multiple extensions.json! Will only use the first.',
      );
    }

    return await loadExtensionsJson(files[0]);
  }
}

export async function loadSettingsJson(
  uri: vscode.Uri,
): Promise<SettingsJson | undefined> {
  const buffer = await vscode.workspace.fs.readFile(uri);
  return hjson.parse(buffer.toString()) as SettingsJson;
}

interface SettingsData {
  shared?: SettingsJson;
  project?: SettingsJson;
  workspace?: SettingsJson;
}

/** Get VSC settings from all potential sources. */
export async function getSettingsData(): Promise<SettingsData> {
  let shared: SettingsJson | undefined;
  let project: SettingsJson | undefined;
  let workspace: SettingsJson | undefined;

  const sharedSettingsFiles = await vscode.workspace.findFiles(
    '.vscode/settings.shared.json',
  );

  if (sharedSettingsFiles.length > 0) {
    const buffer = await vscode.workspace.fs.readFile(sharedSettingsFiles[0]);
    shared = hjson.parse(buffer.toString());
  }

  const projectSettingsFiles = await vscode.workspace.findFiles(
    '.vscode/settings.json',
  );

  if (projectSettingsFiles.length > 0) {
    const buffer = await vscode.workspace.fs.readFile(projectSettingsFiles[0]);
    project = hjson.parse(buffer.toString());
  }

  const workspaceFile = vscode.workspace.workspaceFile;

  if (workspaceFile) {
    const buffer = await vscode.workspace.fs.readFile(workspaceFile);
    const workspaceData = hjson.parse(buffer.toString()) as WorkspaceConfig;
    workspace = workspaceData.settings;
  }

  return { shared, project, workspace };
}

export async function syncSettingsSharedToProject(
  settingsData: SettingsData,
  overwrite = false,
): Promise<void> {
  const { shared, project } = settingsData;

  // If there are no shared settings, there's nothing to sync.
  if (!shared) return;

  logger.info('Syncing shared settings');
  let diff: SettingsJson = {};

  if (!project) {
    // If there are no project settings, just sync all of the shared settings.
    diff = shared;
  } else {
    // Otherwise, sync the differences.
    for (const key of Object.keys(shared)) {
      // If this key isn't in the project settings, copy it over
      if (project[key] === undefined) {
        diff[key] = shared[key];
      }

      // If the setting exists in both places but conflicts, the action we take
      // depends on whether we're *overwriting* (letting the shared setting
      // value take precedence) or not (let the project setting value remain).
      // Letting the project setting remain means doing nothing.
      if (project[key] !== shared[key] && overwrite) {
        diff[key] = shared[key];
      }
    }
  }

  // Apply the different settings.
  for (const [key, value] of Object.entries(diff)) {
    const [category, section] = key.split(/\.(.*)/s, 2);

    try {
      await vscode.workspace.getConfiguration(category).update(section, value);
      logger.info(`==> ${key}: ${value}`);
    } catch (err: unknown) {
      // An error will be thrown if the setting isn't registered (e.g., if
      // it's not a real setting or the extension it pertains to isn't
      // installed). That's fine, just ignore it.
    }
  }

  logger.info('Finished syncing shared settings');
}
