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

import { css, html, LitElement } from "lit";
import { customElement, property } from "lit/decorators.js";

export const DONT_WRAP_LOGS = "Don't wrap logs";
export const WRAP_LOGS = "Wrap logs";

export const RESUME_STREAMING_LOGS = "Resume streaming logs";
export const PAUSE_STREAMING_LOGS = "Pause streaming logs";

const ACTION_ICON_CLASSES = "codicon icons action-icon";

export interface LogViewActionsParameters {
    wrappingLogs?: boolean;
    playActive?: boolean;
}

/**
 * Class to create action buttons that control the log list webview.
 */
@customElement("log-view-actions")
export class LogViewActions extends LitElement {
    static styles = [
        css`
            /* .action-icon {
        border: 1px solid gray;
        min-width: 6rem;
        height: 2rem;
        display: flex;
        align-items: center;
        justify-content: center;
      }

      .action-icon:hover {
        background: var(--vscode-toolbar-hoverBackground);
        border-radius: 5px;
        cursor: pointer;
      } */

            #wrap-logs {
                color: var(--vscode-disabledForeground);
            }

            :host {
                display: flex;
                gap: 1rem;
            }

            :host([wrappingLogs]) #wrap-logs {
                color: var(--vscode-icon-foreground);
            }
        `,
    ];

    @property({ attribute: true, reflect: true, type: Boolean })
    public wrappingLogs: boolean;

    @property({ attribute: true, reflect: true, type: Boolean })
    public playActive = false;

    public static readonly wrapLogsChangeEvent: string = "wrap-logs-change";
    public static readonly clearRequestedEvent: string = "clear-requested";
    public static readonly playPauseChangeEvent: string = "play-pause-change";

    constructor(params: LogViewActionsParameters | undefined) {
        super();
        this.wrappingLogs = params?.wrappingLogs ?? false;
        this.playActive = params?.playActive ?? false;
    }

    render() {
        const wrapLogsTitle = this.wrappingLogs ? DONT_WRAP_LOGS : WRAP_LOGS;
        const playPauseTitle = this.playActive
            ? PAUSE_STREAMING_LOGS
            : RESUME_STREAMING_LOGS;
        // const playPauseIcon = this.playActive ? 'debug-pause' : 'play';

        return html`
            <md-filled-button
                label="Play"
                id="play-pause"
                title="${playPauseTitle}"
                class="${ACTION_ICON_CLASSES}"
                @click="${this.onPlayPauseButtonClick}"
                >Play</md-filled-button
            >

            <md-outlined-button
                label="Clear"
                id="clear"
                title="Clear Logs"
                class="${ACTION_ICON_CLASSES} codicon-clear-all"
                @click="${this.onClearButtonClick}"
                >Clear Logs</md-outlined-button
            >

            <md-outlined-button
                label="Wrap Logs"
                id="wrap-logs"
                title="Wrap Logs"
                class="${ACTION_ICON_CLASSES} codicon-word-wrap"
                @click="${this.onWrapLogsButtonClick}"
                title="${wrapLogsTitle}"
                >Wrap Logs</md-outlined-button
            >
        `;
    }

    private onClearButtonClick() {
        this.dispatchEvent(
            new CustomEvent(LogViewActions.clearRequestedEvent, {
                detail: {},
            })
        );
    }

    private onWrapLogsButtonClick() {
        this.wrappingLogs = !this.wrappingLogs;
        this.dispatchEvent(
            new CustomEvent(LogViewActions.wrapLogsChangeEvent, {
                detail: {
                    wrapLogs: this.wrappingLogs,
                },
            })
        );
    }

    private onPlayPauseButtonClick() {
        this.playActive = !this.playActive;
        this.dispatchEvent(
            new CustomEvent(LogViewActions.playPauseChangeEvent, {
                detail: {
                    playActive: this.playActive,
                },
            })
        );
    }
}
