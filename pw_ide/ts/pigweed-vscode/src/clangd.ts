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

import * as child_process from 'child_process';
import * as fs from 'fs';
import * as fs_p from 'fs/promises';
import * as path from 'path';
import * as readline_p from 'readline/promises';

import * as vscode from 'vscode';
import { Uri } from 'vscode';

import { createHash } from 'crypto';
import { glob } from 'glob';
import * as yaml from 'js-yaml';

import { getReliableBazelExecutable } from './bazel';
import { Disposable } from './disposables';

import {
  didChangeClangdConfig,
  didChangeTarget,
  didInit,
  didUpdateActiveFilesCache,
} from './events';

import { launchTroubleshootingLink } from './links';
import logger from './logging';
import { getPigweedProjectRoot } from './project';
import { OK, RefreshCallback, RefreshManager } from './refreshManager';
import { settingFor, settings, stringSettingFor, workingDir } from './settings';

const CDB_FILE_NAME = 'compile_commands.json' as const;
const CDB_FILE_DIR = '.compile_commands' as const;

// Need this indirection to prevent `workingDir` being called before init.
const CDB_DIR = () => path.join(workingDir.get(), CDB_FILE_DIR);

const clangdPath = () => path.join(workingDir.get(), 'bazel-bin', 'clangd');

const createClangdSymlinkTarget = ':copy_clangd' as const;

/** Create the `clangd` symlink and add it to settings. */
export async function initClangdPath(): Promise<void> {
  logger.info('Ensuring presence of stable clangd symlink');
  const cwd = (await getPigweedProjectRoot(settings, workingDir)) as string;
  const cmd = getReliableBazelExecutable();

  if (!cmd) {
    const message = "Couldn't find a Bazel or Bazelisk executable";
    logger.error(message);
    return;
  }

  const args = ['build', createClangdSymlinkTarget];
  const spawnedProcess = child_process.spawn(cmd, args, { cwd });

  const success = await new Promise<boolean>((resolve) => {
    spawnedProcess.on('spawn', () => {
      logger.info(`Running ${cmd} ${args.join(' ')}`);
    });

    spawnedProcess.stdout.on('data', (data) => logger.info(data.toString()));
    spawnedProcess.stderr.on('data', (data) => logger.info(data.toString()));

    spawnedProcess.on('error', (err) => {
      const { name, message } = err;
      logger.error(`[${name}] while creating clangd symlink: ${message}`);
      resolve(false);
    });

    spawnedProcess.on('exit', (code) => {
      if (code === 0) {
        logger.info('Finished ensuring presence of stable clangd symlink');
        resolve(true);
      } else {
        const message =
          'Failed to ensure presence of stable clangd symlink ' +
          `(error code: ${code})`;

        logger.error(message);
        resolve(false);
      }
    });
  });

  if (!success) return;

  const { update: updatePath } = stringSettingFor('path', 'clangd');
  await updatePath(clangdPath());
}

export const targetPath = (target: string) => path.join(`${CDB_DIR()}`, target);
export const targetCompileCommandsPath = (target: string) =>
  path.join(targetPath(target), CDB_FILE_NAME);

export async function availableTargets(): Promise<string[]> {
  // Get the name of every sub dir in the compile commands dir that contains
  // a compile commands file.
  return (
    (await glob(`**/${CDB_FILE_NAME}`, { cwd: CDB_DIR() }))
      .map((filePath) => path.basename(path.dirname(filePath)))
      // Filter out a catch-all database in the root compile commands dir
      .filter((name) => name.trim() !== '.')
  );
}

export function getTarget(): string | undefined {
  return settings.codeAnalysisTarget();
}

export async function setTarget(
  target: string | undefined,
  settingsFileWriter: (target: string) => Promise<void>,
): Promise<void> {
  target = target ?? getTarget();
  if (!target) return;

  if (!(await availableTargets()).includes(target)) {
    throw new Error(`Target not among available targets: ${target}`);
  }

  await settings.codeAnalysisTarget(target);
  didChangeTarget.fire(target);

  const { update: updatePath } = stringSettingFor('path', 'clangd');
  const { update: updateArgs } = settingFor<string[]>('arguments', 'clangd');

  // These updates all happen asynchronously, and we want to make sure they're
  // all done before we trigger a clangd restart.
  Promise.all([
    updatePath(clangdPath()),
    updateArgs([
      `--compile-commands-dir=${targetPath(target)}`,
      '--query-driver=**',
      '--header-insertion=never',
      '--background-index',
    ]),
    settingsFileWriter(target),
  ]).then(() =>
    // Restart the clangd server so it picks up the new setting.
    vscode.commands.executeCommand('clangd.restart'),
  );
}

/** Parse a compilation database and get the source files in the build. */
async function parseForSourceFiles(target: string): Promise<Set<string>> {
  const rd = readline_p.createInterface({
    input: fs.createReadStream(targetCompileCommandsPath(target)),
    crlfDelay: Infinity,
  });

  const regex = /^\s*"file":\s*"([^"]*)",$/;
  const files = new Set<string>();

  for await (const line of rd) {
    const match = regex.exec(line);

    if (match) {
      const matchedPath = match[1];

      if (
        // Ignore files outside of this project dir
        !path.isAbsolute(matchedPath) &&
        // Ignore build artifacts
        !matchedPath.startsWith('bazel') &&
        // Ignore external dependencies
        !matchedPath.startsWith('external')
      ) {
        files.add(matchedPath);
      }
    }
  }

  return files;
}

// See: https://clangd.llvm.org/config#files
const clangdSettingsDisableFiles = (paths: string[]) => ({
  If: {
    PathExclude: paths,
  },
  Diagnostics: {
    Suppress: '*',
  },
});

export type FileStatus = 'ACTIVE' | 'INACTIVE' | 'ORPHANED';

export class ClangdActiveFilesCache extends Disposable {
  activeFiles: Record<string, Set<string>> = {};

  constructor(refreshManager: RefreshManager<any>) {
    super();
    refreshManager.on(this.refresh, 'didRefresh');
    this.disposables.push(didInit.event(this.refresh));
  }

  /** Get the active files for a particular target. */
  getForTarget = async (target: string): Promise<Set<string>> => {
    if (!Object.keys(this.activeFiles).includes(target)) {
      return new Set();
    }

    return this.activeFiles[target];
  };

  /** Get all the targets that include the provided file. */
  targetsForFile = (fileName: string): string[] =>
    Object.entries(this.activeFiles)
      .map(([target, files]) => (files.has(fileName) ? target : undefined))
      .filter((it) => it !== undefined);

  fileStatus = async (projectRoot: string, target: string, uri: Uri) => {
    const fileName = path.relative(projectRoot, uri.fsPath);
    const activeFiles = await this.getForTarget(target);
    const targets = this.targetsForFile(fileName);

    const status: FileStatus =
      // prettier-ignore
      activeFiles.has(fileName) ? 'ACTIVE' :
      targets.length === 0 ? 'ORPHANED' : 'INACTIVE';

    return {
      status,
      targets,
    };
  };

  refresh: RefreshCallback = async () => {
    logger.info('Refreshing active files cache');
    const targets = await availableTargets();

    const targetSourceFiles = await Promise.all(
      targets.map(
        async (target) => [target, await parseForSourceFiles(target)] as const,
      ),
    );

    this.activeFiles = Object.fromEntries(targetSourceFiles);
    logger.info('Finished refreshing active files cache');
    didUpdateActiveFilesCache.fire();
    return OK;
  };

  writeToSettings = async (target?: string) => {
    const settingsPath = path.join(workingDir.get(), '.clangd');
    const sharedSettingsPath = path.join(workingDir.get(), '.clangd.shared');

    // If the setting to disable code intelligence for files not in the build
    // of this target is disabled, then we need to:
    // 1. *Not* add configuration to disable clangd for any files
    // 2. *Remove* any prior such configuration that may have existed
    if (!settings.disableInactiveFileCodeIntelligence()) {
      await handleInactiveFileCodeIntelligenceEnabled(
        settingsPath,
        sharedSettingsPath,
      );

      return;
    }

    if (!target) return;

    // Create clangd settings that disable code intelligence for all files
    // except those that are in the build for the specified target.
    const activeFilesForTarget = [...(await this.getForTarget(target))];
    let data = yaml.dump(clangdSettingsDisableFiles(activeFilesForTarget));

    // If there are other clangd settings for the project, append this fragment
    // to the end of those settings.
    if (fs.existsSync(sharedSettingsPath)) {
      const sharedSettingsData = (
        await fs_p.readFile(sharedSettingsPath)
      ).toString();
      data = `${sharedSettingsData}\n---\n${data}`;
    }

    await fs_p.writeFile(settingsPath, data, { flag: 'w+' });

    logger.info(
      `Updated .clangd to exclude files not in the build for: ${target}`,
    );
  };
}

/** Show a checkmark next to the item if it's the current setting. */
function markIfActive(active: boolean): vscode.ThemeIcon | undefined {
  return active ? new vscode.ThemeIcon('check') : undefined;
}

export async function setCompileCommandsTarget(
  activeFilesCache: ClangdActiveFilesCache,
) {
  const currentTarget = getTarget();

  const targets = (await availableTargets()).sort().map((target) => ({
    label: target,
    iconPath: markIfActive(target === currentTarget),
  }));

  if (targets.length === 0) {
    vscode.window
      .showErrorMessage("Couldn't find any targets!", 'Get Help')
      .then((selection) => {
        switch (selection) {
          case 'Get Help': {
            launchTroubleshootingLink('bazel-no-targets');
            break;
          }
        }
      });

    return;
  }

  vscode.window
    .showQuickPick(targets, {
      title: 'Select a target',
      canPickMany: false,
    })
    .then(async (selection) => {
      if (!selection) return;
      const { label: target } = selection;
      await setTarget(target, activeFilesCache.writeToSettings);
    });
}

export const setCompileCommandsTargetOnSettingsChange =
  (activeFilesCache: ClangdActiveFilesCache) =>
  (e: vscode.ConfigurationChangeEvent) => {
    if (e.affectsConfiguration('pigweed')) {
      setTarget(undefined, activeFilesCache.writeToSettings);
    }
  };

export async function refreshCompileCommandsAndSetTarget(
  refresh: () => void,
  refreshManager: RefreshManager<any>,
  activeFilesCache: ClangdActiveFilesCache,
) {
  refresh();
  await refreshManager.waitFor('didRefresh');
  await setCompileCommandsTarget(activeFilesCache);
}

/**
 * Handle the case where inactive file code intelligence is enabled.
 *
 * When this setting is enabled, we don't want to disable clangd for any files.
 * That's easy enough, but we also need to revert any configuration we created
 * while the setting was disabled (in other words, while we were disabling
 * clangd for certain files). This handles that and ends up at one of two
 * outcomes:
 *
 * - If there's a `.clangd.shared` file, that will become `.clangd`
 * - If there's not, `.clangd` will be removed
 */
async function handleInactiveFileCodeIntelligenceEnabled(
  settingsPath: string,
  sharedSettingsPath: string,
) {
  if (fs.existsSync(sharedSettingsPath)) {
    if (!fs.existsSync(settingsPath)) {
      // If there's a shared settings file, but no active settings file, copy
      // the shared settings file to make an active settings file.
      await fs_p.copyFile(sharedSettingsPath, settingsPath);
    } else {
      // If both shared settings and active settings are present, check if they
      // are identical. If so, no action is required. Otherwise, copy the shared
      // settings file over the active settings file.
      const settingsHash = createHash('md5').update(
        await fs_p.readFile(settingsPath),
      );
      const sharedSettingsHash = createHash('md5').update(
        await fs_p.readFile(sharedSettingsPath),
      );

      if (settingsHash !== sharedSettingsHash) {
        await fs_p.copyFile(sharedSettingsPath, settingsPath);
      }
    }
  } else if (fs.existsSync(settingsPath)) {
    // If there's no shared settings file, then we just need to remove the
    // active settings file if it's present.
    await fs_p.unlink(settingsPath);
  }
}

export async function disableInactiveFileCodeIntelligence(
  activeFilesCache: ClangdActiveFilesCache,
) {
  logger.info('Disabling inactive file code intelligence');
  await settings.disableInactiveFileCodeIntelligence(true);
  didChangeClangdConfig.fire();
  await activeFilesCache.writeToSettings(settings.codeAnalysisTarget());
  await vscode.commands.executeCommand('clangd.restart');
}

export async function enableInactiveFileCodeIntelligence(
  activeFilesCache: ClangdActiveFilesCache,
) {
  logger.info('Enabling inactive file code intelligence');
  await settings.disableInactiveFileCodeIntelligence(false);
  didChangeClangdConfig.fire();
  await activeFilesCache.writeToSettings();
  await vscode.commands.executeCommand('clangd.restart');
}
