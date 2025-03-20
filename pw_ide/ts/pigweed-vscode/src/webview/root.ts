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

import { html, css, LitElement } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';

interface vscode {
  postMessage(message: any): void;
}
// declare const vscode: vscode;
declare function acquireVsCodeApi(): vscode;
type ExtensionData = {
  recommended: { id: string; installed: boolean; name: string }[];
  unwanted: { id: string; installed: boolean; name: string }[];
};

type CipdReport = {
  bazelPath?: string;
  targetSelected?: string;
  isCompileCommandsGenerated?: boolean;
  compileCommandsPath?: string;
};

const vscode = acquireVsCodeApi();

@customElement('app-root')
export class Root extends LitElement {
  static styles = css`
    :host {
      display: block;
      border: solid 1px gray;
      padding: 16px;
      max-width: 800px;
    }
  `;

  @state() extensionData: ExtensionData = { unwanted: [], recommended: [] };
  @state() cipdReport: CipdReport = {};

  createRenderRoot() {
    return this;
  }

  render() {
    return html`
      <div>
        <details class="vscode-collapsible" open>
          <summary>
            <i class="codicon codicon-chevron-right icon-arrow"></i>
            <b class="title"> Recommended Extensions </b>
          </summary>
          <div>
            <b>Recommended Extensions</b><br/>
            <div class="container">
              ${
                this.extensionData.recommended.length === 0 &&
                html` <p><i>No recommended extensions found.</i></p> `
              }
              ${this.extensionData.recommended.map(
                (ext) =>
                  html`<div class="row">
                    <div>${ext.name || ext.id}</div>
                    <div>
                      ${!ext.installed
                        ? html`
                            <button
                              class="vscode-button"
                              @click="${() => {
                                vscode.postMessage({
                                  type: 'openExtension',
                                  data: ext.id,
                                });
                              }}"
                            >
                              Install
                            </button>
                          `
                        : html`<i>Installed</i>`}
                    </div>
                  </div>`,
              )}
            </div>
            <b>Unwanted Extensions</b><br/>
            <div class="container">
              ${
                this.extensionData.unwanted.length === 0 &&
                html` <p><i>No unwanted extensions found.</i></p> `
              }
              ${this.extensionData.unwanted.map(
                (ext) =>
                  html`<div class="row">
                    <div>${ext.name || ext.id}</div>
                    <div>
                      ${ext.installed
                        ? html`
                            <button
                              class="vscode-button"
                              @click="${() => {
                                vscode.postMessage({
                                  type: 'openExtension',
                                  data: ext.id,
                                });
                              }}"
                            >
                              Remove
                            </button>
                          `
                        : html`<i>Not Installed</i>`}
                    </div>
                  </div>`,
              )}
            </div>
          </div>
        </details>
        <details class="vscode-collapsible" open>
          <summary>
            <i class="codicon codicon-chevron-right icon-arrow"></i>
            <b class="title"> Pigweed Extension Logs </b>
          </summary>
          <div class="container">
            <div class="row">
              <div>
                <p>
                  Dump extension logs and your workspace settings to a file.
                </p>
              </div>
              <div>
                <button
                  class="vscode-button"
                  @click="${() => {
                    vscode.postMessage({ type: 'dumpLogs' });
                  }}">
                  Dump
                </button>
              </div>
            </div>
          </div>
        </details>
        <details class="vscode-collapsible">
          <summary>
            <i class="codicon codicon-chevron-right icon-arrow"></i>
            <b class="title"> Clangd Dashboard </b>
          </summary>
        <div>
          <span>Settings for code navigation.</span>
          <div class="container">
            <div class="row">
              <div>
                <b>Bazel is available</b><br/>
                <sub>${this.cipdReport.bazelPath || 'N/A'}</sub>
              </div>
              <div>${this.cipdReport.bazelPath ? '✅' : '❌'}</div>
            </div>
            <div class="row">
              <div>
                <b>Target is selected</b><br/>
                <sub>${this.cipdReport.targetSelected || 'None'}</sub>
              </div>
              <div>${this.cipdReport.targetSelected ? '✅' : '❌'}</div>
            </div>
            <div class="row">
              <div>
                <b>compile_commands.json exists</b><br/>
                <sub>${this.cipdReport.compileCommandsPath || 'N/A'}</sub>
              </div>
              <div>
                ${this.cipdReport.isCompileCommandsGenerated ? '✅' : '❌'}
              </div>
            </div>
            <div class="row">
            <div><b>Still not working?</b><br/></div>
            </div>
            <div class="row">
              <div>
                Refresh the compile_commands.json
              </div>
              <div>
                <button
                  class="vscode-button"
                  @click="${() => {
                    vscode.postMessage({
                      type: 'refreshCompileCommands',
                    });
                  }}"
                >
                  Refresh
                </button>
              </div>
            </div>
            <div class="row">
              <div>
                Restart clangd language server
              </div>
              <div>
                <button
                  class="vscode-button"
                  @click="${() => {
                    vscode.postMessage({
                      type: 'restartClangd',
                    });
                  }}"
                >
                  Restart
                </button>
              </div>
            </div>
          </div>
        </div>
        </detail>
      </div>
    `;
  }

  async firstUpdated() {
    window.addEventListener(
      'message',
      (e: any) => {
        const message = e.data;
        const { type } = message;
        if (type === 'extensionData') {
          this.extensionData = message.data;
        } else if (type === 'cipdReport') {
          this.cipdReport = message.data;
        }
      },
      false,
    );

    vscode.postMessage({ type: 'getExtensionData' });
    vscode.postMessage({ type: 'getCipdReport' });
  }
}
