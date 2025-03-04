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
import logging from './logging';
import { getSettingsData } from './configParsing';

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
          this._view?.webview.postMessage({
            type: 'extensionData',
            data: report,
          });
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
      }
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
