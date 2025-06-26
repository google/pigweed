# Copyright 2025 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Generates compile_commands.json from bazel aquery and cquery."""

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import concurrent.futures
from pathlib import Path
from typing import Any, Callable, Dict, List, NamedTuple, Optional, Set, Tuple

import json5  # type: ignore

# Type definitions
CQueryItem = Tuple[str, List[str]]


class EnvVar(NamedTuple):
    key: str
    value: str


class Action(NamedTuple):
    target_id: int
    target_label: str
    action_key: str
    mnemonic: str
    configuration_id: int
    arguments: List[str]
    environment_variables: List[EnvVar]
    discovers_inputs: bool
    execution_platform: str
    platform: Optional[str]


class AQueryOutput(NamedTuple):
    actions: List[Action]
    targets: List[Dict[str, Any]]


class ParsedBazelCommand(NamedTuple):
    targets: List[str]
    args: List[str]


class CompileCommand(NamedTuple):
    file: str
    directory: str
    arguments: List[str]


class CompilationDatabase:
    def __init__(self) -> None:
        self.db: List[CompileCommand] = []

    def add(self, command: CompileCommand) -> None:
        self.db.append(command)

    def to_json(self) -> str:
        return json.dumps([c._asdict() for c in self.db], indent=2)


class CompilationDatabaseMap:
    def __init__(self) -> None:
        self.maps: Dict[str, CompilationDatabase] = {}

    def __len__(self) -> int:
        return len(self.maps)

    def get(self, platform: str) -> Optional[CompilationDatabase]:
        return self.maps.get(platform)

    def set(self, platform: str, db: CompilationDatabase) -> None:
        self.maps[platform] = db

    def write_all(self, cwd: str, cdb_file_dir: str, cdb_filename: str) -> None:
        for platform, db in self.maps.items():
            platform_dir = Path(cwd) / cdb_file_dir / platform
            platform_dir.mkdir(parents=True, exist_ok=True)
            (platform_dir / cdb_filename).write_text(db.to_json())


class LoggerUI:
    """A basic logger UI for displaying status and errors."""

    def __init__(self) -> None:
        self.stdout_buffer: List[str] = []
        self.stderr_buffer: List[str] = []
        self._is_tty = sys.stderr.isatty()

    def add_stdout(self, data: str) -> None:
        # Buffer stdout but don't display it on success, mimicking the TS
        # version.
        if data.strip():
            self.stdout_buffer.append(data.strip())

    def add_stderr(self, data: str) -> None:
        # Buffer stderr for display on error.
        if data.strip():
            self.stderr_buffer.append(data.strip())

    def update_status(self, message: str) -> None:
        if self._is_tty:
            # \r = carriage return, \x1b[K = clear line from cursor to end
            sys.stderr.write(f'\r{message}\x1b[K')
            sys.stderr.flush()
        else:
            # For non-TTY environments (like logs), just print the status.
            sys.stderr.write(f'{message}\n')

    def finish(self, message: str) -> None:
        if self._is_tty:
            # Clear the status line and print the final success message.
            sys.stderr.write(f'\r{message}\x1b[K\n')
        else:
            sys.stderr.write(f'{message}\n')
        sys.stderr.flush()

    def finish_with_error(self, message: str) -> None:
        if self._is_tty:
            # Clear the status line and print the final error message.
            sys.stderr.write(f'\r{message}\x1b[K\n')
        else:
            sys.stderr.write(f'{message}\n')

        # Dump the buffered stderr content.
        for line in self.stderr_buffer:
            sys.stderr.write(f'{line}\n')
        sys.stderr.flush()

    def cleanup(self) -> None:
        # In a more complex TUI, we might restore cursor visibility here.
        # For this implementation, it's not needed.
        pass


def ensure_external_workspaces_link_exists(
    cwd: str, logger: Optional[LoggerUI] = None
) -> None:
    """Postcondition: Either //external points into Bazel's fullest set of
    external workspaces in output_base, or we've exited with an error that'll
    help the user resolve the issue.
    """
    is_windows = sys.platform == 'win32'
    source = Path(cwd) / 'external'

    if not (Path(cwd) / 'bazel-out').exists():
        raise RuntimeError(
            f"bazel-out is missing at: {Path(cwd) / 'bazel-out'}"
        )

    # Traverse into output_base via bazel-out, keeping the workspace
    # position-independent, so it can be moved without rerunning
    dest = (
        Path(os.readlink(Path(cwd) / 'bazel-out'))
        .resolve()
        .parent.parent.parent
        / 'external'
    )

    if source.exists():
        if logger:
            logger.add_stdout('Removing existing //external symlink')
        if source.is_dir() and not source.is_symlink():
            shutil.rmtree(source)
        else:
            source.unlink()

    if not source.exists():
        if is_windows:
            # We create a junction on Windows because symlinks need more than
            # default permissions (ugh). Without an elevated prompt or a system
            # in developer mode, symlinking would fail with get "OSError:
            # [WinError 1314] A required privilege is not held by the client:"
            subprocess.run(
                ['mklink', '/J', str(source), str(dest)],
                check=True,
                shell=True,
            )
        else:
            if logger:
                logger.add_stdout(f'symlinking {dest}->{source}')
            source.symlink_to(dest, target_is_directory=True)
        if logger:
            logger.add_stdout('Automatically added //external workspace link.')


def infer_platform_of_action(action: Dict) -> Optional[str]:
    """Infers the platform from the action's arguments.

    This is a heuristic that assumes the platform name is the second component
    of the first `bazel-out` path found in the arguments when searching in
    reverse. This tends to be reliable because output files for a specific
    platform are placed in a directory structure like
    `bazel-out/<platform>/...`.
    """
    for arg in reversed(action['arguments']):
        if arg.startswith(f'bazel-out{os.path.sep}'):
            return arg.split(os.path.sep)[1]
    return None


def get_source_file(args: List[str]) -> str:
    """Heuristically determines the source file from compile command arguments.

    This function is necessary because Bazel's `aquery` output for a
    `CppCompile` action does not explicitly label which argument is the source
    file. This function finds the source file by looking for the first argument
    that doesn't start with a '-' and has a common C/C++ source file
    extension.
    """
    source_extensions = ['.c', '.cc', '.cpp', '.cxx', '.c++', '.m', '.mm']
    candidates = [
        arg
        for arg in args
        if not arg.startswith('-') and Path(arg).suffix in source_extensions
    ]
    if not candidates:
        raise ValueError(
            f'No source file candidate found in arguments: {json.dumps(args)}'
        )
    if len(candidates) == 1:
        return candidates[0]
    # If multiple candidates, try a heuristic: if '-o' is present, use the
    # argument before it.
    try:
        idx_o = args.index('-o')
        if idx_o > 0:
            return args[idx_o - 1]
    except ValueError:
        pass
    return candidates[0]


def run_cquery(
    bazel_cmd: str,
    cwd: str,
    bazel_targets: List[str],
    bazel_args: List[str],
    tui_manager: Optional[LoggerUI] = None,
) -> Tuple[str, int]:
    """Runs `bazel cquery` to get the set of header files for each target."""
    bzl_path = Path(__file__).parent.parent / 'scripts' / 'cquery.bzl'
    args = [
        bazel_cmd,
        'cquery',
        *bazel_args,
        # Query for all dependencies to get a complete list of headers.
        f"deps(set({' '.join(bazel_targets)}))",
        # Use a custom starlark script to extract header file paths.
        '--output=starlark',
        f'--starlark:file={bzl_path}',
        # Keep going to get partial results even if some targets fail.
        '--keep_going',
        # Include the platform in output paths to distinguish architectures.
        # We compile a different compile commands database for each platform.
        # TODO(b/427525027): We should remove this and bail if not set by
        # project.
        '--experimental_platform_in_output_dir',
    ]
    proc = subprocess.Popen(
        args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    output_lines = []
    if proc.stdout:
        for line in proc.stdout:
            output_lines.append(line)
            if tui_manager:
                tui_manager.add_stdout(line)
    output = "".join(output_lines)
    if proc.stderr and tui_manager:
        for line in proc.stderr:
            tui_manager.add_stderr(line)

    proc.wait()
    return output, proc.returncode


def parse_cquery_output(output: str) -> List[CQueryItem]:
    """Parses the raw string output from a cquery command."""
    return [json.loads(line) for line in output.strip().split('\n')]


def run_aquery(
    bazel_cmd: str,
    cwd: str,
    bazel_target: str,
    bazel_args: List[str],
    tui_manager: Optional[LoggerUI] = None,
) -> Tuple[str, int]:
    """Runs `bazel aquery` to get detailed information about build actions."""
    args = [
        bazel_cmd,
        'aquery',
        # Get the full command-line arguments for each compile action.
        '--include_commandline',
        '--output=jsonproto',
        *bazel_args,
        # Filter the build actions to only C++ compilation steps for the
        # target and its dependencies.
        f'mnemonic("CppCompile", deps("{bazel_target}"))',
        # Exclude the full list of input and output files (known as
        # "artifacts") for each action. For a CppCompile action, this includes
        # all source files, headers, and the resulting object file. Including
        # them can make the aquery output extremely large and slow to process.
        # This script gets all the necessary information more efficiently:
        #   - Header files are collected from a separate `cquery`.
        #   - The primary source file is inferred from the action's arguments.
        '--include_artifacts=false',
        # Suppress informational UI messages to keep logs clean.
        '--ui_event_filters=-info',
        # Disable compiler parameter files (@-files) for both target and host
        # actions. This forces Bazel to write the full command line, which is
        # what clangd needs.
        '--features=-compiler_param_file',
        '--host_features=-compiler_param_file',
        # Disable layering checks, which are irrelevant for this tool.
        '--features=-layering_check',
        '--host_features=-layering_check',
        # Include the platform in output paths to distinguish architectures.
        # TODO(b/427525027): We should remove this and bail if not set by
        # project.
        '--experimental_platform_in_output_dir',
        # Keep going to get partial results even if some targets fail.
        '--keep_going',
    ]
    proc = subprocess.Popen(
        args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    output_lines = []
    if proc.stdout:
        for line in proc.stdout:
            output_lines.append(line)
            if tui_manager:
                tui_manager.add_stdout(line)
    output = "".join(output_lines)
    if proc.stderr and tui_manager:
        for line in proc.stderr:
            tui_manager.add_stderr(line)

    proc.wait()
    return output, proc.returncode


def parse_aquery_output(output: str) -> List[Dict]:
    """Parses the raw JSON proto output from an aquery command."""
    aquery_json = json.loads(output)
    if not aquery_json.get('actions'):
        return []
    actions = aquery_json['actions']
    targets_by_id = {t['id']: t for t in aquery_json['targets']}
    for action in actions:
        action['targetLabel'] = targets_by_id[action['targetId']]['label']
        action['platform'] = infer_platform_of_action(action)
    return actions


def get_cpp_command_for_files(
    headers_by_target: Dict[str, List[str]],
    action: Dict,
    tui_manager: Optional[LoggerUI] = None,
) -> Dict[str, Any]:
    """Creates a single compile command for a CppCompile action.

    This is needed because a single `CppCompile` action from `aquery` can
    be responsible for compiling a source file as well as providing the right
    context for its headers. This function creates a single command that can
    be used for all of them.
    """
    if 'arguments' not in action or not isinstance(action['arguments'], list):
        raise ValueError('Invalid compile action: missing arguments')

    args = list(action['arguments'])
    source_file = ''
    try:
        source_file = get_source_file(args)
    except ValueError as e:
        if tui_manager:
            tui_manager.add_stderr(f'Warning: {e}')

    if source_file and not Path(source_file).exists():
        if tui_manager:
            message = f'Warning: Source file {source_file} does not exist.'
            if source_file.startswith('bazel-out'):
                message += ' It may be generated.'
            tui_manager.add_stderr(message)

    header_files = headers_by_target.get(action['targetLabel'], [])
    return {
        'source_files': [source_file],
        'header_files': header_files,
        'arguments': args,
    }


def resolve_virtual_includes_to_real_paths(
    parsed_headers: List[CQueryItem],
) -> Dict[str, str]:
    """Builds a map from Bazel's `_virtual_includes` paths to real paths.

    Bazel uses `_virtual_includes` directories to stage generated headers and
    prevent include path conflicts. Tools like `clangd` don't understand these
    virtual paths and need them to be resolved to the actual source file paths
    in the workspace.

    This function creates a translation map by finding pairs of virtual and
    real header paths for a given build target and deducing the directory
    mapping based on their common suffixes. This map is then used to patch the
    `-I` include paths in the final compile commands.
    """
    virtual_to_real_map: Dict[str, str] = {}
    marker = f'{os.path.sep}_virtual_includes{os.path.sep}'

    for _, headers in parsed_headers:
        for current_header in headers:
            if marker in current_header:
                parts = current_header.split(marker)
                # This indicates a malformed or unexpected virtual include path
                # that the logic can't handle. Skip to the next header.
                if len(parts) < 2 or not parts[1]:
                    continue

                path_before_virtual = parts[0]
                path_after_virtual = parts[1]
                path_after_virtual_parts = path_after_virtual.split(os.path.sep)

                # This case implies path_after_virtual was empty or started
                # with a separator in an unexpected way.
                if (
                    not path_after_virtual_parts
                    or not path_after_virtual_parts[0]
                ):
                    continue

                virtual_subdir = path_after_virtual_parts[0]
                actual_file_suffix_parts = path_after_virtual_parts[1:]
                real_path_suffix = os.path.sep + os.path.join(
                    *actual_file_suffix_parts
                )
                map_key = path_before_virtual + marker + virtual_subdir

                for potential_real_path in headers:
                    if (
                        marker not in potential_real_path
                        and potential_real_path.endswith(real_path_suffix)
                    ):
                        map_value = potential_real_path[
                            : -len(real_path_suffix)
                        ]
                        virtual_to_real_map[map_key] = map_value
                        break
    return virtual_to_real_map


def apply_virtual_include_fix(
    action: CompileCommand, virtual_to_real_map: Dict[str, str]
) -> CompileCommand:
    """Translates Bazel's virtual include paths to real-world paths.

    This is necessary because clangd and other tools do not understand Bazel's
    `_virtual_includes` directories, which are used to stage generated headers
    and prevent include path conflicts. This function replaces the virtual
    paths with the actual source file paths in the workspace, allowing tools
    to correctly resolve includes.
    """
    if not action.arguments:
        return action

    marker = f'{os.path.sep}_virtual_includes{os.path.sep}'
    include_paths_seen: Set[str] = set()
    new_args: List[str] = []

    for arg in action.arguments:
        # Not an include path, so just add it and continue.
        if not arg.startswith('-I'):
            new_args.append(arg)
            continue

        # It is an include path.
        current_include_path = arg[2:]

        # If it's a virtual include path that we can resolve, update it to
        # the real path.
        if marker in arg:
            real_path = virtual_to_real_map.get(current_include_path)
            # Only replace the virtual path if the real path actually exists.
            if real_path and Path(real_path).exists():
                current_include_path = real_path

        # Add the (potentially replaced) path if we haven't seen it before
        # to prevent duplicate include paths.
        if current_include_path not in include_paths_seen:
            new_args.append('-I' + current_include_path)
            include_paths_seen.add(current_include_path)

    return action._replace(arguments=new_args)


def generate_compile_commands_from_aquery_cquery(
    cwd: str,
    aquery_actions: List[Dict],
    cquery_json: List[CQueryItem],
    tui_manager: Optional[LoggerUI] = None,
) -> CompilationDatabaseMap:
    """Generates compile commands from aquery and cquery output."""
    virtual_to_real_map = resolve_virtual_includes_to_real_paths(cquery_json)
    headers_by_target: Dict[str, List[str]] = {}
    for target, headers in cquery_json:
        clean_target = target[2:] if target.startswith('@@') else target
        headers_by_target[clean_target] = headers

    possible_platforms = sorted(
        list(
            {
                action['platform']
                for action in aquery_actions
                if action.get('platform')
            }
        )
    )

    compile_commands_per_platform = CompilationDatabaseMap()

    for platform in possible_platforms:
        compile_commands = CompilationDatabase()
        header_files_seen: Set[str] = set()

        for action in aquery_actions:
            if action.get('platform') != platform:
                continue
            command_data = get_cpp_command_for_files(
                headers_by_target, action, tui_manager
            )
            files = list(command_data['source_files'])
            for hdr in command_data['header_files']:
                if hdr not in header_files_seen:
                    header_files_seen.add(hdr)
                    files.append(hdr)

            for file in files:
                if not file:
                    continue
                cmd = CompileCommand(
                    file=file,
                    directory=cwd,
                    arguments=list(command_data['arguments']),
                )
                fixed_cmd = apply_virtual_include_fix(cmd, virtual_to_real_map)
                compile_commands.add(fixed_cmd)

        compile_commands_per_platform.set(platform, compile_commands)
        if tui_manager:
            tui_manager.add_stdout(
                f'Finished processing platform {platform} with '
                f'{len(compile_commands.db)} commands.'
            )

    return compile_commands_per_platform


def delete_files_in_subdir(parent_dir_path: str, file_name_to_delete: str):
    """Deletes files with a specific name in immediate subdirectories."""
    parent_dir = Path(parent_dir_path)
    if not parent_dir.exists():
        return
    for sub_dir in parent_dir.iterdir():
        if not sub_dir.is_dir():
            continue

        file_to_delete = sub_dir / file_name_to_delete
        if not file_to_delete.exists():
            continue

        try:
            file_to_delete.unlink()
        except OSError as e:
            print(
                f'  Error processing file {file_to_delete}: {e}',
                file=sys.stderr,
            )


def generate_compile_commands(
    bazel_cmd: str,
    cwd: str,
    cdb_file_dir: str,
    cdb_filename: str,
    bazel_targets: List[str],
    bazel_args: List[str],
    aquery_runner: Callable,
    cquery_runner: Callable,
    tui_manager: Optional[LoggerUI] = None,
) -> None:
    """Generates compile commands by running aquery and cquery."""
    start_time = os.times().elapsed
    ensure_external_workspaces_link_exists(cwd, tui_manager)

    aquery_actions: List[Dict] = []
    with concurrent.futures.ThreadPoolExecutor() as executor:
        future_to_target: Dict[concurrent.futures.Future, str] = {
            executor.submit(
                aquery_runner, bazel_cmd, cwd, target, bazel_args, tui_manager
            ): target
            for target in bazel_targets
        }
        for future in concurrent.futures.as_completed(future_to_target):
            target = future_to_target[future]
            try:
                aquery_output, aquery_returncode = future.result()
                if aquery_returncode != 0:
                    if tui_manager:
                        tui_manager.add_stderr(
                            f'aquery failed for {target} with exit code '
                            f'{aquery_returncode}.'
                        )
                    # This is a critical error, so we should stop here.
                    raise RuntimeError(
                        f'aquery failed for {target} with exit code '
                        f'{aquery_returncode}.'
                    )

                aquery_actions.extend(parse_aquery_output(aquery_output))
            except RuntimeError as exc:
                if tui_manager:
                    tui_manager.add_stderr(
                        f'{target} generated an exception: {exc}'
                    )
                # Re-raise the exception to stop the entire process
                raise exc

    cquery_json: List[CQueryItem] = []
    cquery_output, cquery_returncode = cquery_runner(
        bazel_cmd, cwd, bazel_targets, bazel_args, tui_manager
    )
    if cquery_returncode == 0:
        try:
            cquery_json = parse_cquery_output(cquery_output)
        except json.JSONDecodeError as e:
            if tui_manager:
                tui_manager.add_stderr(f'Failed to parse cquery output: {e}')
    elif tui_manager:
        tui_manager.add_stderr(
            f'cquery failed with exit code {cquery_returncode}.'
        )

    compile_commands_per_platform = (
        generate_compile_commands_from_aquery_cquery(
            cwd, aquery_actions, cquery_json, tui_manager
        )
    )

    if len(compile_commands_per_platform) > 0:
        if tui_manager:
            tui_manager.update_status(
                f'⏳ Cleaning output directory: {cdb_file_dir}'
            )
        full_cdb_dir_path = Path(cwd) / cdb_file_dir
        delete_files_in_subdir(str(full_cdb_dir_path), cdb_filename)
        full_cdb_dir_path.mkdir(exist_ok=True)

        compile_commands_per_platform.write_all(cwd, cdb_file_dir, cdb_filename)

        if tui_manager:
            end_time = os.times().elapsed
            tui_manager.add_stdout(
                f'Finished generating {cdb_filename} for '
                f'{len(compile_commands_per_platform)} platforms in '
                f'{round((end_time - start_time) * 1000)}ms.'
            )
    elif tui_manager:
        tui_manager.add_stdout('No compile commands generated.')


def _handle_error_and_exit(message: str, tui_manager: LoggerUI) -> None:
    """Logs a final error message and exits the program."""
    tui_manager.finish_with_error(message)
    sys.exit(1)


def generate_compile_commands_with_status(
    bazel_cmd: str,
    cwd: str,
    cdb_file_dir: str,
    cdb_filename: str,
    bazel_targets: List[str],
    bazel_args: List[str],
    tui_manager: LoggerUI,
) -> None:
    """Generates compile commands and displays status updates."""
    targets_str = ', '.join(bazel_targets)
    plural = 's' if len(bazel_targets) > 1 else ''
    tui_manager.update_status(
        f'⏳ Generating {cdb_filename} for target{plural}: {targets_str}'
    )
    try:
        generate_compile_commands(
            bazel_cmd,
            cwd,
            cdb_file_dir,
            cdb_filename,
            bazel_targets,
            bazel_args,
            run_aquery,
            run_cquery,
            tui_manager,
        )
        tui_manager.finish(f'✅ Generated {cdb_filename} successfully.')
    except RuntimeError as e:
        _handle_error_and_exit(
            f'❌ Error generating {cdb_filename}: {e}', tui_manager
        )
    finally:
        tui_manager.cleanup()


def parse_bazel_build_command(
    command: str, bazel_path: str, cwd: str
) -> ParsedBazelCommand:
    """Parses a Bazel command string to extract targets and canonicalize
    arguments.

    This function tokenizes the input command, identifies the Bazel subcommand
    (e.g., build, test), and separates recognized Bazel targets (those starting
    with `//` or `:`) from other arguments. It then uses
    `bazel canonicalize-flags` with the identified subcommand to normalize the
    remaining arguments.
    """
    trimmed_command = command.strip()
    if not trimmed_command:
        raise ValueError(f'Invalid bazel command (empty): {command}')

    parts = shlex.split(trimmed_command)
    if not parts:
        raise ValueError(f'Invalid bazel command (empty): {command}')

    sub_command = parts[0]
    potential_targets_and_args = parts[1:]

    targets: List[str] = []
    potential_args: List[str] = []

    for part in potential_targets_and_args:
        if part.startswith(('//', ':', '@')):
            targets.append(part)
        elif part == '--':
            break
        else:
            potential_args.append(part)

    if not targets:
        raise ValueError(
            'Could not find any bazel targets (starting with // or :) in '
            f'command: {command}'
        )

    canonicalized_args: List[str] = []
    if potential_args:
        canonicalize_cmd_args = [
            bazel_path,
            'canonicalize-flags',
            f'--for_command={sub_command}',
            '--',
            *potential_args,
        ]
        try:
            result = subprocess.run(
                canonicalize_cmd_args,
                cwd=cwd,
                check=True,
                capture_output=True,
                text=True,
            )
        except subprocess.CalledProcessError as e:
            raise RuntimeError(
                'bazel canonicalize-flags failed with exit code '
                f'{e.returncode}:\n{e.stderr}'
            ) from e

        canonicalized_args = [
            line.strip() for line in result.stdout.strip().split('\n') if line
        ]

    return ParsedBazelCommand(targets=targets, args=canonicalized_args)


def save_last_bazel_command_in_user_settings(
    cwd: str, bazel_cmd: str, tui_manager: Optional[LoggerUI] = None
) -> None:
    """Saves the last bazel command to .vscode/settings.json."""
    settings_path = Path(cwd) / '.vscode' / 'settings.json'
    if not settings_path.exists():
        return

    content = settings_path.read_text()
    settings = json5.loads(content)
    settings['pigweed.bazelCompileCommandsLastBuildCommand'] = bazel_cmd
    settings_path.write_text(json5.dumps(settings, indent=2))
    if tui_manager:
        tui_manager.add_stdout(
            'Saved last bazel command to user settings.json.'
        )


def run_as_cli() -> None:
    """Command-line interface for generating compile commands."""
    tui_manager = LoggerUI()
    parser = argparse.ArgumentParser(
        description='Generate compile commands for Bazel projects.'
    )
    parser.add_argument(
        '--target', required=True, help='Bazel build command to run.'
    )
    parser.add_argument(
        '--cwd', required=True, help='Working directory for bazel.'
    )
    parser.add_argument(
        '--bazelCmd', required=True, help='Path to the bazel executable.'
    )
    parser.add_argument(
        '--cdbFileDir', default='.compile_commands', help='Output directory.'
    )
    parser.add_argument(
        '--cdbFilename',
        default='compile_commands.json',
        help='Output filename.',
    )
    args = parser.parse_args()

    tui_manager.update_status('⏳ Parsing Bazel command...')

    try:
        parsed_cmd = parse_bazel_build_command(
            args.target, args.bazelCmd, args.cwd
        )
    except (ValueError, RuntimeError) as e:
        _handle_error_and_exit(f'❌ Error parsing command: {e}', tui_manager)

    generate_compile_commands_with_status(
        args.bazelCmd,
        args.cwd,
        args.cdbFileDir,
        args.cdbFilename,
        parsed_cmd.targets,
        parsed_cmd.args,
        tui_manager,
    )
    try:
        save_last_bazel_command_in_user_settings(
            args.cwd, args.target, tui_manager
        )
    except (OSError, ValueError) as e:
        tui_manager.add_stderr(
            'Warning: Failed to save last bazel command to user '
            f'settings.json: {e}'
        )


if __name__ == '__main__':
    run_as_cli()
