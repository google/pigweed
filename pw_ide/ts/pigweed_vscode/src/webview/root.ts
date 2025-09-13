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
  clangdPath?: string;
  bazelPath?: string;
  targetSelected?: string;
  isCompileCommandsGenerated?: boolean;
  compileCommandsPath?: string;
  isBazelInterceptorEnabled?: boolean;
  bazelCompileCommandsManualBuildCommand?: string;
  bazelCompileCommandsLastBuildCommand?: string;
  experimentalCompileCommands?: boolean;
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
  @state() manualBazelTarget = '';

  createRenderRoot() {
    return this;
  }

  connectedCallback() {
    super.connectedCallback();
    // Initialize manualBazelTarget when component connects
    this.manualBazelTarget =
      this.cipdReport.bazelCompileCommandsManualBuildCommand || '';
  }

  private _toggleBazelInterceptor(enabled: boolean) {
    vscode.postMessage({
      type: enabled
        ? 'enableBazelBuildInterceptor'
        : 'disableBazelBuildInterceptor', // Send appropriate message
    });
  }

  private _toggleExperimentalCompileCommands(enabled: boolean) {
    vscode.postMessage({
      type: 'setExperimentalCompileCommands',
      data: enabled,
    });
  }

  // Handles input changes for the manual bazel command
  private _handleManualBazelInputChange(event: Event) {
    const inputElement = event.target as HTMLInputElement;
    this.manualBazelTarget = inputElement.value;
  }

  // Called when the Refresh button is clicked
  private _refreshCompileCommandsManually(e?: MouseEvent) {
    e?.stopPropagation();
    vscode.postMessage({
      type: 'refreshCompileCommandsManually',
      data: this.manualBazelTarget.trim(),
    });
  }

  private get _isCodeIntelligenceHealthy(): boolean {
    return !!(
      this.cipdReport.clangdPath &&
      this.cipdReport.bazelPath &&
      this.cipdReport.targetSelected &&
      this.cipdReport.isCompileCommandsGenerated
    );
  }

  render() {
    const currentManualTarget =
      this.manualBazelTarget ??
      (this.cipdReport.bazelCompileCommandsManualBuildCommand || '');

    return html`
      <div>
        <details class="vscode-collapsible">
          <summary>
            <i class="codicon codicon-chevron-right icon-arrow"></i>
            <b class="title"> Recommended Extensions</b>
          </summary>
          <div>
            <b>Recommended Extensions</b><br />
            <div class="container">
              ${this.extensionData.recommended.length === 0 &&
              html` <p><i>No recommended extensions found.</i></p> `}
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
            <b>Unwanted Extensions</b><br />
            <div class="container">
              ${this.extensionData.unwanted.length === 0 &&
              html` <p><i>No unwanted extensions found.</i></p> `}
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
        <details class="vscode-collapsible">
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
                  }}"
                >
                  Dump
                </button>
              </div>
            </div>
          </div>
        </details>
        <details class="vscode-collapsible">
          <summary>
            <i class="codicon codicon-chevron-right icon-arrow"></i>
            <b class="title"> Code Intelligence </b>
          </summary>
          <div>
            <span>Settings for code navigation and intelligence.</span>
            <div class="container">
              <div class="toggle-button-group">
                <button
                  class="toggle-button ${this.cipdReport
                    .isBazelInterceptorEnabled
                    ? 'active'
                    : ''}"
                  @click="${() => {
                    this._toggleBazelInterceptor(true); // Enable
                  }}"
                >
                  <b>Last <code>bazel build</code> command</b>
                  <br />
                  <span
                    >Intercepts Bazel commands to update code intelligence
                    automatically. Only files built by the command will have
                    active intelligence.</span
                  >
                </button>
                <div
                  role="button"
                  tabindex="0"
                  @keydown=${(e: KeyboardEvent) => {
                    if (e.key === 'Enter') this._toggleBazelInterceptor(true);
                  }}
                  class="toggle-button ${!this.cipdReport
                    .isBazelInterceptorEnabled
                    ? 'active'
                    : ''}"
                  @click="${() => {
                    this._toggleBazelInterceptor(false); // Disable
                  }}"
                >
                  <b>Manual with a fixed command</b>
                  <br />
                  <span
                    >In this mode, code intelligence is updated only for the
                    below command, and must be done manually.</span
                  >
                  <br />
                  <div class="input-button-row">
                    <span class="prefix">bazel build </span>
                    <input
                      type="text"
                      class="vscode-input"
                      @click=${(e: MouseEvent) => e.stopPropagation()}
                      .value=${currentManualTarget}
                      @input=${this._handleManualBazelInputChange}
                      placeholder="//..."
                      aria-label="Manual bazel build target"
                      ?disabled=${this.cipdReport.isBazelInterceptorEnabled}
                    />
                    <div
                      class="vscode-button input-button"
                      role="button"
                      href="#"
                      @click=${this._refreshCompileCommandsManually}
                      ?disabled=${this.cipdReport.isBazelInterceptorEnabled}
                      @keydown=${(e: KeyboardEvent) => {
                        if (e.key === ' ' || e.key === 'Enter')
                          this._refreshCompileCommandsManually();
                      }}
                    >
                      Refresh
                    </div>
                  </div>
                </div>
              </div>
              <div class="row">
                <label class="checkbox-label">
                  <input
                    type="checkbox"
                    .checked=${this.cipdReport.experimentalCompileCommands}
                    @change=${(e: Event) =>
                      this._toggleExperimentalCompileCommands(
                        (e.target as HTMLInputElement).checked,
                      )}
                  />
                  Use aspect-based compile commands generator (experimental)
                </label>
              </div>
              <div class="row">
                <div>
                  <b>Compile commands generated using</b><br />
                  <sub>
                    ${this.cipdReport.bazelCompileCommandsLastBuildCommand
                      ? `bazel ${this.cipdReport.bazelCompileCommandsLastBuildCommand}`
                      : 'N/A'}
                  </sub>
                </div>
                <div></div>
              </div>

              ${this._isCodeIntelligenceHealthy
                ? html`
                    <div class="row">
                      <div><b>Everything appears to be working</b><br /></div>
                      <div>✅</div>
                    </div>
                  `
                : html` <div class="row">
                    <div>
                      <b>Still not working?</b><br />
                      <span>See below on what might be wrong.</span>
                    </div>
                  </div>`}

              <details
                class="vscode-collapsible"
                ?open=${!this._isCodeIntelligenceHealthy}
              >
                <summary>
                  <i class="codicon codicon-chevron-right icon-arrow"></i>
                  <b class="title"> Debug Code Intelligence </b>
                </summary>
                <div>
                  <div class="row">
                    <div>Restart clangd language server</div>
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
                  <div class="row">
                    <div>
                      <b>Clangd is available</b><br />
                      <sub>${this.cipdReport.clangdPath || 'N/A'}</sub>
                    </div>
                    <div>${this.cipdReport.clangdPath ? '✅' : '❌'}</div>
                  </div>
                  <div class="row">
                    <div>
                      <b>Bazel is available</b><br />
                      <sub>${this.cipdReport.bazelPath || 'N/A'}</sub>
                    </div>
                    <div>${this.cipdReport.bazelPath ? '✅' : '❌'}</div>
                  </div>
                  <div class="row">
                    <div>
                      <b>Target is selected</b><br />
                      <sub>${this.cipdReport.targetSelected || 'None'}</sub>
                    </div>
                    <div>${this.cipdReport.targetSelected ? '✅' : '❌'}</div>
                  </div>
                  <div class="row">
                    <div>
                      <b>compile_commands.json exists</b><br />
                      <sub>${this.cipdReport.compileCommandsPath || 'N/A'}</sub>
                    </div>
                    <div>
                      ${this.cipdReport.isCompileCommandsGenerated
                        ? '✅'
                        : '❌'}
                    </div>
                  </div>
                </div>
              </details>
            </div>
          </div>
        </details>
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
          this.manualBazelTarget =
            this.cipdReport.bazelCompileCommandsManualBuildCommand || '';
          this.requestUpdate();
        }
      },
      false,
    );

    vscode.postMessage({ type: 'getExtensionData' });
    vscode.postMessage({ type: 'getCipdReport' });
  }
}
