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

  @property() name = 'World';
  @state() count = 0;
  @state() extensionData: ExtensionData = { unwanted: [], recommended: [] };

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
            <table style="width: 100%;">
              ${this.extensionData.recommended.map(
                (ext) =>
                  html`<tr class="content-row">
                    <td>${ext.name || ext.id}</td>
                    <td>
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
                    </td>
                  </tr>`,
              )}
            </table>
          </div>
        </details>

        <details class="vscode-collapsible" open>
          <summary>
            <i class="codicon codicon-chevron-right icon-arrow"></i>
            <b class="title"> Unwanted Extensions </b>
          </summary>
          <div>
            <table style="width: 100%;">
              ${this.extensionData.unwanted.map(
                (ext) =>
                  html`<tr class="content-row">
                    <td>${ext.name || ext.id}</td>
                    <td>
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
                    </td>
                  </tr>`,
              )}
            </table>
          </div>
        </details>
      </div>
    `;
  }

  modify(val: number) {
    this.count += val;
  }

  async firstUpdated() {
    window.addEventListener(
      'message',
      (e: any) => {
        const message = e.data;
        const { type } = message;
        if (type === 'extensionData') {
          this.extensionData = message.data;
        }
      },
      false,
    );

    vscode.postMessage({ type: 'getExtensionData' });
  }
}
