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
import * as child_process from 'child_process';
import * as path from 'path';
import * as fs from 'fs';
import { getReliableBazelExecutable } from '../bazel';
import logger from '../logging';
import { getPigweedProjectRoot } from '../project';
import { settings, workingDir } from '../settings/vscode';
import { OK, RefreshCallbackResult } from '../refreshManager';
import {
  CompilationDatabase,
  CompilationDatabaseMap,
  CompileCommand,
} from './parser';

type CQueryItem = [string, string[]];

type EnvVar = {
  key: string;
  value: string;
};

type Action = {
  targetId: number;
  targetLabel: string;
  actionKey: string;
  mnemonic: string;
  configurationId: number;
  arguments: string[];
  environmentVariables: EnvVar[];
  discoversInputs: boolean;
  executionPlatform: string;
  platform: string | undefined;
};

type AQueryOutput = {
  actions: Action[];
  targets: [
    {
      id: number;
      label: string;
    },
  ];
};

/**
 * Postcondition: Either //external points into Bazel's fullest set of external workspaces in output_base, or we've exited with an error that'll help the user resolve the issue.
 */
function ensureExternalWorkspacesLink_exists() {
  const cwd = workingDir.get();
  const isWindows = process.platform === 'win32';
  const source = path.resolve(path.join(cwd, 'external'));

  if (!fs.existsSync(path.join(cwd, 'bazel-out'))) {
    logger.error('//bazel-out is missing.');
    process.exit(1);
  }

  // Traverse into output_base via bazel-out, keeping the workspace position-independent, so it can be moved without rerunning
  const dest = path.resolve(
    fs.readlinkSync(path.resolve(path.join(cwd, 'bazel-out'))),
    '..',
    '..',
    '..',
    'external',
  );

  // Handle problem cases where //external exists
  if (fs.existsSync(source) && fs.lstatSync(source)) {
    logger.info('Removing existing //external symlink');
    fs.unlinkSync(source);
  }

  // Create link if it doesn't already exist
  if (!fs.existsSync(source)) {
    if (isWindows) {
      // We create a junction on Windows because symlinks need more than default permissions (ugh). Without an elevated prompt or a system in developer mode, symlinking would fail with get "OSError: [WinError 1314] A required privilege is not held by the client:"
      child_process.execSync(`mklink /J "${source}" "${dest}"`); // shell required for mklink builtin
    } else {
      logger.info('symlinking ' + dest + '->' + source);
      fs.symlinkSync(dest, source, 'dir');
    }
    logger.info('Automatically added //external workspace link.');
  }
}

export function inferPlatformOfAction(action: Action) {
  // Start by looking from the end of args.
  for (let i = action.arguments.length - 1; i >= 0; i--) {
    if (action.arguments[i].startsWith('bazel-out' + path.sep)) {
      return action.arguments[i].split(path.sep)[1];
    }
  }
  return undefined;
}

/**
 * Apply general fixes by removing Bazel‑specific flags.
 * For example, remove module cache paths, debug prefix maps, and flags that clangd cannot understand.
 */
function allPlatformPatch(args: string[]) {
  const filtered = [];
  for (const arg of args) {
    if (arg.startsWith('-fmodules-cache-path=bazel-out/')) continue;
    if (arg.startsWith('-fdebug-prefix-map')) continue;
    if (arg === '-fno-canonical-system-headers') continue;
    filtered.push(arg);
  }
  // Also remove any "-gcc-toolchain" flag and its following argument.
  const result = [];
  let skipNext = false;
  for (const arg of filtered) {
    if (skipNext) {
      skipNext = false;
      continue;
    }
    if (arg === '-gcc-toolchain') {
      skipNext = true;
      continue;
    }
    if (arg.startsWith('-gcc-toolchain=')) continue;
    result.push(arg);
  }
  return result;
}

/**
 * Heuristically determine the source file from the compile command arguments.
 * Returns the first argument that does not start with '-' and has a recognized source extension.
 */
function getSourceFile(args: string[]) {
  const sourceExtensions = ['.c', '.cc', '.cpp', '.cxx', '.c++', '.m', '.mm'];
  const candidates = args.filter((arg) => {
    if (arg.startsWith('-')) return false;
    const ext = path.extname(arg);
    return sourceExtensions.includes(ext);
  });
  if (candidates.length === 0) {
    throw new Error(
      'No source file candidate found in arguments: ' + JSON.stringify(args),
    );
  }
  if (candidates.length === 1) return candidates[0];
  // If multiple candidates, try a heuristic: if '-o' is present, use the argument before it.
  const idxO = args.indexOf('-o');
  if (idxO > 0) {
    return args[idxO - 1];
  }
  return candidates[0];
}

async function runCquery(cwd: string): Promise<CQueryItem[]> {
  const cmd = getReliableBazelExecutable();

  if (!cmd) {
    throw new Error("Couldn't find a Bazel or Bazelisk executable");
  }

  const bzlPath = path.join(__dirname, '..', 'scripts', 'cquery.bzl');
  const args = [
    'cquery',
    'deps(//...)',
    '--output=starlark',
    '--starlark:file=' + bzlPath,
    '--keep_going',
    '--experimental_platform_in_output_dir',
  ];
  const spawnedProcess = child_process.spawn(cmd, args, { cwd });

  const headersArray = await new Promise<CQueryItem[] | null>((resolve) => {
    let output = '';

    spawnedProcess.stdout.on('data', (data) => (output += data.toString()));
    spawnedProcess.stderr.on('data', (data) => logger.info(data.toString()));

    spawnedProcess.on('exit', (code) => {
      try {
        const parsedHeaders: CQueryItem[] = output
          .trim()
          .split('\n')
          .map((l) => JSON.parse(l));
        resolve(parsedHeaders);
      } catch (e) {
        const message = 'Failed to run cquery ' + `(error code: ${code})`;
        logger.error(message);
        resolve(null);
      }
    });
  });

  if (!headersArray) throw new Error('Running cquery failed.');
  return headersArray;
}

async function runAquery(cwd: string): Promise<Action[]> {
  const cmd = getReliableBazelExecutable();

  if (!cmd) {
    throw new Error("Couldn't find a Bazel or Bazelisk executable");
  }
  const args = [
    'aquery',
    '--include_commandline',
    '--output=jsonproto',
    'mnemonic("CppCompile", "//...")',
    '--include_artifacts=false',
    '--ui_event_filters=-info',
    '--features=-compiler_param_file',
    '--features=-layering_check',
    '--host_features=-compiler_param_file',
    '--host_features=-layering_check',
    '--experimental_platform_in_output_dir',
    '--keep_going',
  ];
  const spawnedProcess = child_process.spawn(cmd, args, { cwd });

  const outputJson = await new Promise<Action[] | null>((resolve) => {
    let output = '';

    spawnedProcess.stdout.on('data', (data) => (output += data.toString()));
    spawnedProcess.stderr.on('data', (data) => logger.info(data.toString()));

    spawnedProcess.on('exit', (code) => {
      try {
        const aqueryJson: AQueryOutput = JSON.parse(output);
        const actions = aqueryJson.actions.map((action: Action) => {
          action.targetLabel = aqueryJson.targets.find(
            (target) => target.id === action.targetId,
          )!.label;
          action.platform = inferPlatformOfAction(action);
          return action;
        });
        resolve(actions);
      } catch (e) {
        const message = 'Failed to run aquery ' + `(error code: ${code})`;

        logger.error(message);
        resolve(null);
      }
    });
  });

  if (!outputJson) throw new Error('Running aquery failed.');
  return outputJson;
}

/**
 * Process a compile action and return an object with:
 *  - sourceFiles: an array with the source file path,
 *  - headerFiles: an array of header dependency paths,
 *  - arguments: the cleaned compile command arguments.
 */
function getCppCommandForFiles(
  headersByTarget: { [key: string]: string[] },
  action: Action,
) {
  if (!action || !Array.isArray(action.arguments)) {
    throw new Error('Invalid compile action: missing arguments');
  }
  // Convert Bazel’s environmentVariables to an object.
  // action.env = convertEnvVariables(action.environmentVariables);
  // Start with a copy of the arguments.
  let args = [...action.arguments];
  // Apply general Bazel-to-clang flag cleanup.
  args = allPlatformPatch(args);
  // Determine the source file.
  let sourceFile;
  try {
    sourceFile = getSourceFile(args);
  } catch (err: any) {
    logger.warn('Warning: ' + err.message);
    sourceFile = '';
  }
  if (sourceFile && !fs.existsSync(sourceFile)) {
    logger.warn(
      'Warning: Source file ' +
        sourceFile +
        ' does not exist. It may be generated.',
    );
  }
  // Resolve header dependencies via dependency generation.
  const headerFiles: string[] = headersByTarget[action.targetLabel] || [];
  return {
    sourceFiles: [sourceFile],
    headerFiles: headerFiles,
    arguments: args,
  };
}

/** This parses the cquery output and creates a map from virtual_includes to
 * real paths. */
export function resolveVirtualIncludesToRealPaths(parsedHeaders: CQueryItem[]) {
  const virtualToRealMap: { [key: string]: string } = {};
  parsedHeaders.forEach(([_target, headers]) => {
    const virtualIncludesMarker = path.sep + '_virtual_includes' + path.sep;
    // First we check if headers[0] is a header with '/_virtual_includes/'
    let markerPathFound = '';
    for (let i = 0; i < headers.length; i++) {
      if (headers[i].includes(virtualIncludesMarker)) {
        markerPathFound = headers[i];
        break;
      }
    }
    if (!markerPathFound) return;

    // Now we split the marker path and get suffix
    const [_markerPathPrefix, markerPathSuffix] = markerPathFound.split(
      virtualIncludesMarker,
    );
    // We further need to remove first part of suffix
    const markerPathSuffixParts = markerPathSuffix.split(path.sep);
    markerPathSuffixParts.shift();
    const realPathSuffix = path.sep + markerPathSuffixParts.join(path.sep);
    const realPathPrefix = markerPathFound.slice(0, -realPathSuffix.length);

    // Now we search for adjacent headers with this suffix
    for (let i = 0; i < headers.length; i++) {
      if (
        !headers[i].includes(virtualIncludesMarker) &&
        headers[i].endsWith(realPathSuffix)
      ) {
        // We save this real path minus the suffix
        virtualToRealMap[realPathPrefix] = headers[i].slice(
          0,
          -realPathSuffix.length,
        );
        break;
      }
    }
  });
  return virtualToRealMap;
}

export function applyVirtualIncludeFix(
  action: CompileCommand,
  virtualToRealMap: { [key: string]: string },
) {
  if (!action.data.arguments) return action;
  const virtualIncludesMarker = path.sep + '_virtual_includes' + path.sep;
  const includePathsSeen = new Set();
  action.data.arguments = action.data.arguments
    .map((arg: string) => {
      if (
        arg.startsWith('-I') &&
        arg.includes(virtualIncludesMarker) &&
        virtualToRealMap[arg.slice(2)]
      ) {
        return '-I' + virtualToRealMap[arg.slice(2)];
      }
      return arg;
    })
    // Deduplicate the include paths
    .filter((arg: string) => {
      if (arg.startsWith('-I') && includePathsSeen.has(arg.slice(2))) {
        return false;
      } else if (arg.startsWith('-I')) {
        includePathsSeen.add(arg.slice(2));
      }
      return true;
    });
  return action;
}

export async function generateCompileCommandsFromAqueryCquery(
  cwd: string,
  aqueryActions: Action[],
  cqueryJson: CQueryItem[],
) {
  const virtualToRealMap = resolveVirtualIncludesToRealPaths(cqueryJson);
  const headersByTarget: { [key: string]: string[] } = {};
  cqueryJson.forEach(([target, headers]) => {
    headersByTarget[target] = headers;
  });

  const possiblePlatforms = [
    ...new Set(
      aqueryActions
        .map((action) => action.platform)
        .filter((platform) => platform !== undefined),
    ),
  ];

  const compileCommandsPerPlatform = new CompilationDatabaseMap();

  possiblePlatforms.forEach((platform) => {
    const compileCommands = new CompilationDatabase();
    const headerFilesSeen = new Set();

    aqueryActions.forEach((action: Action) => {
      if (action.platform !== platform) return;
      const {
        sourceFiles,
        headerFiles,
        arguments: cmdArgs,
      } = getCppCommandForFiles(headersByTarget, action);
      const files = [...sourceFiles];
      // Include header entries if not already added.
      for (const hdr of headerFiles) {
        if (!headerFilesSeen.has(hdr)) {
          headerFilesSeen.add(hdr);
          files.push(hdr);
        }
      }
      for (const file of files) {
        compileCommands.add(
          applyVirtualIncludeFix(
            new CompileCommand({
              file: file,
              directory: cwd,
              arguments: cmdArgs,
            }),
            virtualToRealMap,
          ),
        );
      }
    });

    compileCommandsPerPlatform.set(platform, compileCommands);
    logger.info(
      'Finished generating compile_commands.json for platform ' +
        platform +
        ' with ' +
        compileCommands.db.length +
        ' commands.',
    );
  });

  return compileCommandsPerPlatform;
}

export async function generateCompileCommands(): Promise<RefreshCallbackResult> {
  const startTime = Date.now();
  logger.info('Generating compile_commands.json.');
  const cwd = (await getPigweedProjectRoot(settings, workingDir)) as string;
  ensureExternalWorkspacesLink_exists();
  const aqueryActions = await runAquery(cwd);
  const cqueryJson = await runCquery(cwd);

  const compileCommandsPerPlatform =
    await generateCompileCommandsFromAqueryCquery(
      cwd,
      aqueryActions,
      cqueryJson,
    );

  await compileCommandsPerPlatform.writeAll();

  logger.info(
    'Finished generating compile_commands.json for ' +
      compileCommandsPerPlatform.size +
      ' platforms in ' +
      (Date.now() - startTime) +
      'ms.',
  );

  // Restart the clangd server so it picks up the new compile commands.
  vscode.commands.executeCommand('clangd.restart');
  return OK;
}
