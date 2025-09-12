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

import * as child_process from 'child_process';
import * as path from 'path';
import * as fs from 'fs';
import { modify, applyEdits, FormattingOptions } from 'jsonc-parser';
import { Logger } from '../loggingTypes';
import {
  CompilationDatabase,
  CompilationDatabaseMap,
  CompileCommand,
} from './parser';
import { CDB_FILE_DIR, LAST_BAZEL_COMMAND_FILE_NAME } from './paths';
import { LoggerUI, UIManager } from './compileCommandsGeneratorUI';

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

interface ParsedBazelCommand {
  targets: string[];
  args: string[];
}

/**
 * Postcondition: Either //external points into Bazel's fullest set of external workspaces in output_base, or we've exited with an error that'll help the user resolve the issue.
 */
function ensureExternalWorkspacesLink_exists(cwd: string, logger?: Logger) {
  const isWindows = process.platform === 'win32';
  const source = path.resolve(path.join(cwd, 'external'));

  if (!fs.existsSync(path.join(cwd, 'bazel-out'))) {
    logger?.error('bazel-out is missing at: ' + path.join(cwd, 'bazel-out'));
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
    logger?.info('Removing existing //external symlink');
    fs.unlinkSync(source);
  }

  // Create link if it doesn't already exist
  if (!fs.existsSync(source)) {
    if (isWindows) {
      // We create a junction on Windows because symlinks need more than default permissions (ugh). Without an elevated prompt or a system in developer mode, symlinking would fail with get "OSError: [WinError 1314] A required privilege is not held by the client:"
      child_process.execSync(`mklink /J "${source}" "${dest}"`); // shell required for mklink builtin
    } else {
      logger?.info('symlinking ' + dest + '->' + source);
      fs.symlinkSync(dest, source, 'dir');
    }
    logger?.info('Automatically added //external workspace link.');
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

async function runCquery(
  bazelCmd: string,
  cwd: string,
  bazelTargets: string[],
  bazelArgs: string[],
  tuiManager?: UIManager | LoggerUI,
): Promise<CQueryItem[]> {
  const bzlPath = path.join(__dirname, '..', 'scripts', 'cquery.bzl');
  const args = [
    'cquery',
    ...bazelArgs,
    `deps(set(${bazelTargets.join(' ')}))`,
    '--output=starlark',
    '--starlark:file=' + bzlPath,
    '--keep_going',
    '--experimental_platform_in_output_dir',
  ];
  const spawnedProcess = child_process.spawn(bazelCmd, args, { cwd });

  const headersArray = await new Promise<CQueryItem[] | null>((resolve) => {
    let output = '';

    spawnedProcess.stdout.on('data', (data) => {
      const dataStr = data.toString();
      output += dataStr; // Accumulate for JSON parsing
      tuiManager?.addStdout(dataStr); // Also send to TUI
    });
    spawnedProcess.stderr.on(
      'data',
      (data) => tuiManager?.addStderr(data.toString()),
    );

    spawnedProcess.on('exit', (code) => {
      if (code !== 0) {
        // Log error through TUI Manager's stderr handler before rejecting/resolving null
        const message = `cquery failed with exit code ${code}. Attempting to use partial output...`;
        tuiManager?.addStderr(message);
      }
      try {
        const parsedHeaders: CQueryItem[] = output
          .trim()
          .split('\n')
          .map((l) => JSON.parse(l));
        resolve(parsedHeaders);
      } catch (e) {
        const message =
          'Failed to parse cquery output ' +
          `(error code: ${code}): ${
            e instanceof Error ? e.message : String(e)
          }`;
        tuiManager?.addStderr(message); // Log parse error via TUI
        resolve(null);
      }
    });

    spawnedProcess.on('error', (err) => {
      // Handle spawn errors
      const message = `Failed to start cquery process: ${err.message}`;
      tuiManager?.addStderr(message);
      resolve(null);
    });
  });

  return headersArray || [];
}

async function runAquery(
  bazelCmd: string,
  cwd: string,
  bazelTarget: string,
  bazelArgs: string[],
  tuiManager?: UIManager | LoggerUI,
): Promise<Action[]> {
  const args = [
    'aquery',
    '--include_commandline',
    '--output=jsonproto',
    ...bazelArgs,
    `mnemonic("CppCompile", deps("${bazelTarget}"))`,
    '--include_artifacts=false',
    '--ui_event_filters=-info',
    '--features=-compiler_param_file',
    '--features=-layering_check',
    '--host_features=-compiler_param_file',
    '--host_features=-layering_check',
    '--experimental_platform_in_output_dir',
    '--keep_going',
  ];
  const spawnedProcess = child_process.spawn(bazelCmd, args, { cwd });

  const outputJson = await new Promise<Action[] | null>((resolve) => {
    let output = '';

    spawnedProcess.stdout.on('data', (data) => {
      const dataStr = data.toString();
      output += dataStr; // Accumulate for JSON parsing
      tuiManager?.addStdout(dataStr);
    });
    spawnedProcess.stderr.on(
      'data',
      (data) => tuiManager?.addStderr(data.toString()),
    );

    spawnedProcess.on('exit', (code) => {
      if (code !== 0) {
        const message = `aquery failed with exit code ${code}. Attempting to use partial output...`;
        tuiManager?.addStderr(message);
      }
      try {
        const aqueryJson: AQueryOutput = JSON.parse(output);
        if (!aqueryJson.actions || aqueryJson.actions.length === 0) {
          resolve([]);
          return;
        }
        const actions = aqueryJson.actions.map((action: Action) => {
          action.targetLabel = aqueryJson.targets.find(
            (target) => target.id === action.targetId,
          )!.label;
          action.platform = inferPlatformOfAction(action);
          return action;
        });
        resolve(actions);
      } catch (e) {
        const message =
          'Failed to parse aquery output ' +
          `(error code: ${code}): ${
            e instanceof Error ? e.message : String(e)
          }`;
        tuiManager?.addStderr(message);
        resolve(null);
      }
    });

    spawnedProcess.on('error', (err) => {
      const message = `Failed to start aquery process: ${err.message}`;
      tuiManager?.addStderr(message);
      resolve(null);
    });
  });

  if (!outputJson)
    throw new Error('Running aquery failed for target ' + bazelTarget + '.');
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
  tuiManager?: UIManager | LoggerUI,
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
    tuiManager?.addStderr('Warning: ' + err.message);
    sourceFile = '';
  }
  if (sourceFile && !fs.existsSync(sourceFile)) {
    tuiManager?.addStderr(
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
  const virtualIncludesMarker = path.sep + '_virtual_includes' + path.sep; // Results in "/_virtual_includes/"

  parsedHeaders.forEach(([_target, headers]) => {
    headers.forEach((currentHeader) => {
      // Iterate through each header path in the current list
      if (currentHeader.includes(virtualIncludesMarker)) {
        const parts = currentHeader.split(virtualIncludesMarker);
        // Expecting two parts: the path before the marker and the path after.
        if (parts.length < 2 || !parts[1]) {
          // Silently skip this potentially malformed virtual include path.
          return; // Equivalent to 'continue' for this iteration of headers.forEach
        }

        const pathBeforeVirtual = parts[0];
        const pathAfterVirtual = parts[1]; // e.g., "config/pw_assert/config.h"

        // Split the pathAfterVirtual to isolate the virtual subdirectory and the common suffix.
        const pathAfterVirtualParts = pathAfterVirtual.split(path.sep);
        // e.g., ["config", "pw_assert", "config.h"]

        if (pathAfterVirtualParts.length === 0 || !pathAfterVirtualParts[0]) {
          // This case implies pathAfterVirtual was empty or started with a separator in an unexpected way.
          return;
        }

        // The first segment after "/_virtual_includes/" is part of the virtual path key.
        const virtualSubdirectoryComponent = pathAfterVirtualParts[0]; // e.g., "config"

        // The remaining segments form the suffix that should match the end of a real path.
        const actualFileSuffixParts = pathAfterVirtualParts.slice(1); // e.g., ["pw_assert", "config.h"]

        // Construct the suffix string to match against real paths.
        // This will be like "/pw_assert/config.h"
        const realPathSuffixToMatch =
          path.sep + actualFileSuffixParts.join(path.sep);

        // If actualFileSuffixParts is empty (e.g., virtual path was ".../_virtual_includes/somefile.h"),
        // realPathSuffixToMatch would become just "/". This logic is consistent with the original code's derivation,
        // though the provided examples all have deeper structures (e.g., ".../_virtual_includes/dir/file.h").
        // The problem's example data structure suggests `actualFileSuffixParts` will typically not be empty.

        // Construct the key for our map. This represents the "virtual directory".
        // e.g., "bazel-out/darwin_arm64-fastbuild/bin/pw_assert/_virtual_includes/config"
        const mapKey =
          pathBeforeVirtual +
          virtualIncludesMarker +
          virtualSubdirectoryComponent;

        // Now, search for a corresponding real path within the same 'headers' list.
        for (let i = 0; i < headers.length; i++) {
          const potentialRealPath = headers[i];
          // A real path must not contain the virtual includes marker itself,
          // and it must end with the derived common suffix.
          if (
            !potentialRealPath.includes(virtualIncludesMarker) &&
            potentialRealPath.endsWith(realPathSuffixToMatch)
          ) {
            // The value for our map is the prefix of this real path.
            const mapValue = potentialRealPath.slice(
              0,
              -realPathSuffixToMatch.length,
            );
            virtualToRealMap[mapKey] = mapValue;
            // Once the corresponding real path is found for the current virtual include,
            // we break this inner loop and move to the next header in headers.forEach.
            break;
          }
        }
      }
    });
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
  tuiManager?: UIManager | LoggerUI,
) {
  const virtualToRealMap = resolveVirtualIncludesToRealPaths(cqueryJson);
  const headersByTarget: { [key: string]: string[] } = {};
  cqueryJson.forEach(([target, headers]) => {
    const cleanTarget = target.startsWith('@@') ? target.slice(2) : target;
    headersByTarget[cleanTarget] = headers;
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
      } = getCppCommandForFiles(headersByTarget, action, tuiManager);
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
              arguments: [...cmdArgs],
            }),
            virtualToRealMap,
          ),
        );
      }
    });

    compileCommandsPerPlatform.set(platform, compileCommands);
    tuiManager?.addStdout(
      'Finished processing platform ' +
        platform +
        ' with ' +
        compileCommands.db.length +
        ' commands.',
    );
  });

  return compileCommandsPerPlatform;
}

export async function generateCompileCommands(
  bazelCmd: string,
  cwd: string,
  cdbFileDir: string,
  cdbFilename: string,
  bazelTargets: string[] = ['//...'],
  bazelArgs: string[] = [],
  tuiManager?: UIManager | LoggerUI,
) {
  const startTime = Date.now();
  ensureExternalWorkspacesLink_exists(cwd);

  const aqueryActions = (
    await Promise.all(
      bazelTargets.map(async (target) =>
        runAquery(bazelCmd, cwd, target, bazelArgs, tuiManager),
      ),
    )
  ).flat();
  const cqueryJson = await runCquery(
    bazelCmd,
    cwd,
    bazelTargets,
    bazelArgs,
    tuiManager,
  );
  const compileCommandsPerPlatform =
    await generateCompileCommandsFromAqueryCquery(
      cwd,
      aqueryActions,
      cqueryJson,
      tuiManager,
    );

  // Only delete previous compile_commands if new one is not empty
  if (compileCommandsPerPlatform.size > 0) {
    // Delete and recreate the compile_commands directory.
    tuiManager?.updateStatus(`⏳ Cleaning output directory: ${cdbFileDir}`);
    const fullCdbDirPath = path.join(cwd, cdbFileDir);
    deleteFilesInSubDir(fullCdbDirPath, 'compile_commands.json');
    fs.mkdirSync(fullCdbDirPath, { recursive: true });

    await compileCommandsPerPlatform.writeAll(cwd, cdbFileDir, cdbFilename);

    tuiManager?.addStdout(
      'Finished generating compile_commands.json for ' +
        compileCommandsPerPlatform.size +
        ' platforms in ' +
        (Date.now() - startTime) +
        'ms.',
    );
  } else {
    tuiManager?.addStdout('No compile commands generated.');
  }
}

export function deleteFilesInSubDir(
  parentDirPath: string,
  fileNameToDelete: string,
) {
  try {
    if (!fs.existsSync(parentDirPath)) {
      return;
    }

    const parentEntries = fs.readdirSync(parentDirPath, {
      withFileTypes: true,
    });

    for (const parentEntry of parentEntries) {
      if (parentEntry.isDirectory()) {
        const potentialFilePath = path.join(
          parentDirPath,
          parentEntry.name,
          fileNameToDelete,
        );
        try {
          if (fs.existsSync(potentialFilePath)) {
            fs.unlinkSync(potentialFilePath);
          }
        } catch (fileOpErr: any) {
          console.error(
            `  Error processing file ${potentialFilePath}: ${fileOpErr.message}`,
          );
        }
      }
    }
  } catch (err: any) {
    console.error(
      `Error processing parent directory ${parentDirPath}: ${err.message}`,
    );
  }
}

export async function generateCompileCommandsWithStatus(
  bazelCmd: string,
  cwd: string,
  cdbFileDir: string,
  cdbFilename: string,
  bazelTargets: string[] = ['//...'],
  bazelArgs: string[] = [],
  tuiManager?: UIManager | LoggerUI,
) {
  tuiManager?.updateStatus(
    `⏳ Generating compile_commands.json for target${
      bazelTargets.length > 1 ? 's' : ''
    }: ${bazelTargets.join(', ')}`,
  );
  generateCompileCommands(
    bazelCmd,
    cwd,
    cdbFileDir,
    cdbFilename,
    bazelTargets,
    bazelArgs,
    tuiManager,
  )
    .then(() => {
      // Success: Use regular finish, which only shows the status.
      tuiManager?.finish('✅ Generated compile_commands.json successfully.');
    })
    .catch((err) => {
      // Error: Use finishWithError to show status and dump stdout/stderr.
      const errorMessage = `❌ Error generating compile_commands.json: ${
        err instanceof Error ? err.message : String(err)
      }`;
      tuiManager?.finishWithError(errorMessage);
      console.error('Error generating compile_commands.json:', err);
      process.exit(1);
    })
    .finally(() => {
      tuiManager?.cleanup();
    });
}

/**
 * Parses a Bazel command string to extract targets and canonicalize arguments.
 *
 * This function tokenizes the input command, identifies the Bazel subcommand (e.g., build, test),
 * and separates recognized Bazel targets (those starting with `//` or `:`) from other arguments.
 * It then uses `bazel canonicalize-flags` with the identified subcommand to normalize the
 * remaining arguments.
 *
 * @param {string} command - The Bazel command string to parse (e.g., "build //foo:bar --config=my_config -c opt").
 * @param {string} bazelPath - The path to the Bazel executable.
 * @param {string} cwd - The current working directory in which to run `bazel canonicalize-flags`.
 * @returns {Promise<ParsedBazelCommand>} A promise that resolves to an object containing:
 * - `targets`: An array of identified Bazel target strings.
 * - `args`: An array of canonicalized Bazel argument strings.
 * @throws {Error} If the command is empty, invalid, contains no targets, or if `bazel canonicalize-flags` fails.
 */
export async function parseBazelBuildCommand(
  command: string,
  bazelPath: string,
  cwd: string,
): Promise<ParsedBazelCommand> {
  // This regex tokenizes a command string, respecting quotes and allowing
  // concatenation of quoted and unquoted parts into single arguments
  // (e.g., `foo="bar baz"` becomes one token). It handles:
  // 1. Unquoted sequences `[^\s"']+`
  // 2. Double-quoted strings `"(?:\\.|[^"\\])*"` (with escapes)
  // 3. Single-quoted strings `'(?:\\.|[^'\\])*'` (with escapes)
  // The outer `(?:...)+` structure groups these if they are adjacent without spaces.
  // Whitespace separates the final tokens. Uses global flag `/g`.
  const tokenizerRegex = /(?:[^\s"']+|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')+/g;
  const trimmedCommand = command.trim();

  // If the command is empty after trimming, handle it early.
  if (trimmedCommand === '') {
    throw new Error(`Invalid bazel command (empty): ${command}`);
  }

  const parts = trimmedCommand.match(tokenizerRegex);

  if (!parts || parts.length === 0 || (parts.length === 1 && parts[0] === '')) {
    throw new Error(`Invalid bazel command (empty): ${command}`);
  }

  // Determine the bazel subcommand (build, run, test, etc.) - needed for canonicalize-flags
  // Assumes the subcommand is the first part after 'bazel'
  const subCommand = parts[0];
  // Filter out the subcommand for further processing
  const potentialTargetsAndArgs = parts.length > 1 ? parts.slice(1) : [];

  // 2. Identify targets and potential arguments
  const targets: string[] = [];
  const potentialArgs: string[] = [];

  for (const part of potentialTargetsAndArgs) {
    // Basic target identification (starts with // or :)
    // More robust parsing might be needed for complex target patterns.
    if (part.startsWith('//') || part.startsWith(':') || part.startsWith('@')) {
      targets.push(part);
    } else if (part == '--') {
      // Discard everything after the `--` separator.
      break;
    } else {
      // Anything else is considered a potential argument for canonicalization
      potentialArgs.push(part);
    }
  }

  // Throw if no target is identified *after* splitting
  if (targets.length === 0) {
    throw new Error(
      `Could not find any bazel targets (starting with // or :) in command: ${command}`,
    );
  }

  // Canonicalize arguments using `bazel canonicalize-flags`.
  // This Bazel command takes a list of flags and returns them in a stable, canonical form.
  // For example, it expands shorthand flags, orders them consistently, and handles flags that
  // might be specified in different ways but mean the same thing.
  //
  // Example:
  // If `subCommand` is 'build' and `potentialArgs` is `['-c', 'opt', '--config=myconfig']`,
  // `bazel canonicalize-flags --for_command=build -- -c opt --config=myconfig`
  // will output:
  //   --compilation_mode=opt
  //   --config=myconfig
  // This step ensures that arguments are processed in a standardized way.
  let canonicalizedArgs: string[] = [];
  if (potentialArgs.length > 0) {
    const canonicalizeCmdArgs = [
      'canonicalize-flags',
      `--for_command=${subCommand}`,
      '--',
      ...potentialArgs,
    ];

    try {
      const result = await new Promise<{ stdout: string; stderr: string }>(
        (resolve, reject) => {
          let stdout = '';
          let stderr = '';
          const spawnedProcess = child_process.spawn(
            bazelPath,
            canonicalizeCmdArgs,
            {
              cwd: cwd, // Run in the specified working directory
              shell: false, // More secure and predictable
              env: process.env, // Inherit environment
            },
          );

          spawnedProcess.stdout.on('data', (data) => {
            stdout += data.toString();
          });

          spawnedProcess.stderr.on('data', (data) => {
            stderr += data.toString();
          });

          spawnedProcess.on('error', (err) => {
            reject(
              new Error(
                `Failed to spawn bazel canonicalize-flags: ${err.message}`,
              ),
            );
          });

          spawnedProcess.on('close', (code) => {
            if (code === 0) {
              resolve({ stdout, stderr });
            } else {
              reject(
                new Error(
                  `bazel canonicalize-flags failed with exit code ${code}:\n${stderr}`,
                ),
              );
            }
          });
        },
      );

      // Process the stdout: split by lines, trim, filter empty
      canonicalizedArgs = result.stdout
        .trim()
        .split('\n')
        .map((line) => line.trim())
        .filter((line) => line !== ''); // Filter out potential empty lines
    } catch (error) {
      // Rethrow or handle the error from the promise/spawn
      throw new Error(
        `Error during bazel canonicalize-flags: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  }

  // 4. Return the results
  return {
    targets: targets,
    args: canonicalizedArgs,
  };
}

export function saveLastBazelCommand(
  cwd: string,
  bazelCmd: string,
  tuiManager?: UIManager | LoggerUI,
) {
  try {
    const compileCommandsDir = path.join(cwd, CDB_FILE_DIR);
    if (!fs.existsSync(compileCommandsDir)) {
      fs.mkdirSync(compileCommandsDir, { recursive: true });
    }
    const filePath = path.join(
      compileCommandsDir,
      LAST_BAZEL_COMMAND_FILE_NAME,
    );
    fs.writeFileSync(filePath, bazelCmd, 'utf-8');
    tuiManager?.addStdout(
      `Saved last bazel command to ${CDB_FILE_DIR}/${LAST_BAZEL_COMMAND_FILE_NAME}.`,
    );
  } catch (e: any) {
    tuiManager?.addStderr('Failed to save last bazel command: ' + e.toString());
  }
}

async function runAsCli() {
  const tuiManager = new UIManager();
  const args = process.argv.slice(2);
  const parsedArgs: { [key: string]: string } = {};

  for (let i = 0; i < args.length; i += 2) {
    const argName = args[i];
    const argValue = args[i + 1];

    if (argName && argName.startsWith('--')) {
      const key = argName.substring(2);
      parsedArgs[key] = argValue;
    }
  }

  if (!parsedArgs['target'] || !parsedArgs['cwd'] || !parsedArgs['bazelCmd']) {
    console.error('Missing required arguments.', parsedArgs);
    process.exit(1);
  }

  tuiManager?.updateStatus('⏳ Parsing Bazel command...');

  const { targets, args: bazelArgs } = await parseBazelBuildCommand(
    parsedArgs['target'],
    parsedArgs['bazelCmd'],
    parsedArgs['cwd'],
  );

  const cdbFileDir = parsedArgs['cdbFileDir'] || CDB_FILE_DIR;

  await generateCompileCommandsWithStatus(
    parsedArgs['bazelCmd'],
    parsedArgs['cwd'],
    cdbFileDir,
    parsedArgs['cdbFilename'] || 'compile_commands.json',
    targets,
    bazelArgs,
    tuiManager,
  );
  saveLastBazelCommand(parsedArgs['cwd'], parsedArgs['target'], tuiManager);
}
if (require.main === module) {
  runAsCli();
}
