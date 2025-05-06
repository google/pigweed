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
import { checkExtensionsAndGetStatus } from './extensionManagement';
import logging, { output } from './logging';
import { getSettingsData } from './configParsing';
import getCipdReport from './clangd/report';
import { existsSync, mkdirSync } from 'fs';
import {
  deleteFilesInSubDir,
  generateCompileCommands,
  generateCompileCommandsWithStatus,
  parseBazelBuildCommand,
} from './clangd/compileCommandsGenerator';
import { getReliableBazelExecutable } from './bazel';
import { settings, workingDir } from './settings/vscode';
import { CDB_FILE_DIR, CDB_FILE_NAME } from './clangd';
import {
  createBazelInterceptorFile,
  deleteBazelInterceptorFile,
  getBazelInterceptorPath,
} from './clangd/compileCommandsUtils';
import { LoggerUI } from './clangd/compileCommandsGeneratorUI';
import path from 'path';

export class WebviewProvider implements vscode.WebviewViewProvider {
  public static readonly viewType = 'pigweed.webview';

  private _view?: vscode.WebviewView;

  constructor(private readonly _extensionUri: vscode.Uri) {}

  public resolveWebviewView(
    webviewView: vscode.WebviewView,
    _context: vscode.WebviewViewResolveContext,
    _token: vscode.CancellationToken,
  ) {
    this._view = webviewView;

    webviewView.webview.options = {
      // Allow scripts in the webview
      enableScripts: true,

      localResourceRoots: [this._extensionUri],
    };

    webviewView.webview.html = this._getHtmlForWebview(webviewView.webview);

    webviewView.webview.onDidReceiveMessage(async (data) => {
      switch (data.type) {
        case 'getExtensionData': {
          const report = await checkExtensionsAndGetStatus();
          logging.info('getExtensionData reported: ' + JSON.stringify(report));
          this._view?.webview.postMessage({
            type: 'extensionData',
            data: report,
          });
          break;
        }
        case 'getCipdReport': {
          await this.sendCipdReport();
          break;
        }
        case 'refreshCompileCommands': {
          vscode.commands.executeCommand('pigweed.refresh-compile-commands');
          break;
        }
        case 'restartClangd': {
          vscode.commands.executeCommand('clangd.restart');
          break;
        }
        case 'openExtension': {
          const extensionId = data.data;
          await vscode.commands.executeCommand('extension.open', extensionId);
          break;
        }
        case 'dumpLogs': {
          const workspaceFolders = vscode.workspace.workspaceFolders;
          if (!workspaceFolders) return {};
          const workspaceFolder = workspaceFolders[0];

          const logsFilePath = vscode.Uri.joinPath(
            workspaceFolder.uri,
            'pigweed-vscode-logs.txt',
          );

          const logs = logging.logs;
          let output = '';
          const settings = await getSettingsData();
          output +=
            'SETTINGS\n========\n' + JSON.stringify(settings, null, 2) + '\n\n';
          output += 'LOGS\n====\n';
          for (const log of logs) {
            output += `[${log.timestamp.toISOString()}] [${log.level.toUpperCase()}] ${
              log.message
            }\n`;
          }
          await vscode.workspace.fs.writeFile(
            logsFilePath,
            Buffer.from(output),
          );
          vscode.window.showInformationMessage(
            'Logs dumped to ' + logsFilePath.fsPath,
          );
          break;
        }
        case 'enableBazelBuildInterceptor': {
          logging.info('Enabling bazel build interceptor');
          await createBazelInterceptorFile();
          settings.disableBazelInterceptor(false);
          vscode.window.showInformationMessage(
            'Bazel build interceptor enabled',
          );
          await this.sendCipdReport();
          break;
        }
        case 'disableBazelBuildInterceptor': {
          logging.info('Disabling bazel build interceptor');
          await deleteBazelInterceptorFile();
          settings.disableBazelInterceptor(true);
          vscode.window.showInformationMessage(
            'Bazel build interceptor disabled',
          );
          await this.sendCipdReport();
          break;
        }

        case 'refreshCompileCommandsManually': {
          const buildCmd = data.data;
          const bazelBinary = getReliableBazelExecutable();
          const cwd = workingDir.get();
          logging.info(`Generating compile commands for ${buildCmd}`);
          if (buildCmd.trim() === '' || !bazelBinary) break;
          output.show();
          const parsedCmd = await parseBazelBuildCommand(
            `build ${buildCmd}`,
            bazelBinary,
            cwd,
          ).catch((e) => {
            logging.error(e.message);
            vscode.window.showErrorMessage(e.message);
          });

          if (!parsedCmd) {
            logging.info('Unable to parse build command.');
            return;
          }

          await settings.bazelCompileCommandsManualBuildCommand(buildCmd);

          logging.info(`Command was parsed to: ${JSON.stringify(parsedCmd)}`);

          // Delete and recreate the compile_commands directory.
          const fullCdbDirPath = path.join(workingDir.get(), CDB_FILE_DIR);
          deleteFilesInSubDir(fullCdbDirPath, 'compile_commands.json');
          mkdirSync(fullCdbDirPath, { recursive: true });

          logging.info('Cleaned compile_commands directory.');

          await generateCompileCommandsWithStatus(
            bazelBinary,
            workingDir.get(),
            CDB_FILE_DIR,
            CDB_FILE_NAME,
            parsedCmd.targets,
            parsedCmd.args,
            new LoggerUI(logging),
          );
        }
      }
    });
  }

  private async sendCipdReport() {
    let report = await getCipdReport();
    const pathForBazelBuildInterceptor = getBazelInterceptorPath();
    if (!pathForBazelBuildInterceptor) return;
    const bazelInterceptorExists = existsSync(pathForBazelBuildInterceptor);
    report = {
      ...report,
      isBazelInterceptorEnabled: bazelInterceptorExists,
    };
    logging.info('getCipdReport reported: ' + JSON.stringify(report));
    this._view?.webview.postMessage({
      type: 'cipdReport',
      data: report,
    });
  }

  private _getHtmlForWebview(webview: vscode.Webview) {
    // Get the local path to main script run in the webview, then convert it to a uri we can use in the webview.
    const scriptUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this._extensionUri, 'dist', 'webview.js'),
    );

    // Use a nonce to only allow a specific script to be run.
    const nonce = getNonce();

    return `<!DOCTYPE html>
			<html lang="en">
			<head>
				<meta charset="UTF-8">
				<meta name="viewport" content="width=device-width, initial-scale=1.0">
				<title>Pigweed</title>
			</head>
			<body>
				<app-root>
                </app-root>

				<script nonce="${nonce}" src="${scriptUri}"></script>
			</body>
			</html>`;
  }
}

function getNonce() {
  let text = '';
  const possible =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  for (let i = 0; i < 32; i++) {
    text += possible.charAt(Math.floor(Math.random() * possible.length));
  }
  return text;
}
