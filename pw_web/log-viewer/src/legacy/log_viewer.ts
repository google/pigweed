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

import { LogControl } from "./log_control";
import { LogViewActions } from "./log_view_actions";
import { LogRowData } from "./models/log_data";
import { State } from "./models/state";
import { LogList } from "./log_list";
import { LogViewOptions } from "./models/fields";
import { LitElement, css } from "lit";
import { customElement } from "lit/decorators.js";

export const RESUMING_LOG_STREAMING_LOG =
    "Resuming log streaming, press [pause icon] to pause.";
export const PAUSING_LOG_STREAMING_LOG =
    "Playback has been paused, press [play icon] to resume.";

export type ExternalActionRequestEvent =
    | { type: "pause-log-streaming" }
    | { type: "clear-logs"; untilTimestamp: number }
    | { type: "fetch-clock-monotonic" }
    | { type: "resume-log-streaming" };

@customElement("fuchsia-log-viewer")
export class FuchsiaLogViewer extends LitElement {
    private logActionContainer: HTMLDivElement;
    private logControl?: LogControl;
    private logList: LogList;
    private logListContainer: HTMLDivElement;
    private logViewActions: LogViewActions;

    public static readonly externalActionRequestEvent: string =
        "external-action-request-event";

    constructor(
        private state: State,
        private options: LogViewOptions = {
            columnFormatter: (_fieldName, text) => text,
            showControls: true,
        }
    ) {
        super();

        this.state = new State();

        this.logList = new LogList(
            this.state.currentFields,
            this.state.shouldWrapLogs,
            this.state.setHeaderWidth.bind(this.state),
            this.state.currentFilter,
            options
        );

        this.logActionContainer = document.createElement("div");
        this.logActionContainer.id = "log-action-container";
        this.logActionContainer.className = "log-action-container";
        this.logListContainer = document.createElement("div");
        this.logListContainer.id = "log-list-container";

        if (this.options.showControls) {
            this.logControl = new LogControl(this.state.currentFilterText);
            this.logControl.addEventListener(
                LogControl.filterChangeEvent,
                (e: Event) => {
                    const event = e as CustomEvent;
                    const { filter, text } = event.detail;
                    this.state.registerFilter(filter, text);
                    this.logControl!.resultsMessage = filter.isEmpty()
                        ? ""
                        : `${this.logList.filterResultsCount} results`;
                }
            );
        }

        this.state.addEventListener("filterChange", (e: Event) => {
            const event = e as CustomEvent;
            this.logList.filterChangeHandler(event);
        });

        this.logViewActions = new LogViewActions({
            wrappingLogs: this.state.shouldWrapLogs,
            playActive: true,
        });
        this.logList.addEventListener(
            FuchsiaLogViewer.externalActionRequestEvent,
            (e: Event) => {
                this.dispatchEvent(
                    new CustomEvent(e.type, {
                        detail: (e as CustomEvent).detail,
                    })
                );
            }
        );
        this.logViewActions.addEventListener(
            LogViewActions.clearRequestedEvent,
            () => {
                const detail: ExternalActionRequestEvent = {
                    type: "clear-logs",
                    untilTimestamp: this.logList.lastTimestamp,
                };
                this.dispatchEvent(
                    new CustomEvent(
                        FuchsiaLogViewer.externalActionRequestEvent,
                        {
                            detail,
                        }
                    )
                );
                this.logList.reset();
            }
        );
        this.logViewActions.addEventListener(
            LogViewActions.wrapLogsChangeEvent,
            (e) => {
                const event = e as CustomEvent;
                const { wrapLogs } = event.detail;
                this.logList.logWrapping = wrapLogs;
                this.state.shouldWrapLogs = wrapLogs;
            }
        );
        this.logViewActions.addEventListener(
            LogViewActions.playPauseChangeEvent,
            (e) => {
                const event = e as CustomEvent;
                const { playActive } = event.detail;
                this.onPlayPauseChangeEvent(playActive);
            }
        );
    }

    static styles = [
        css`
            .log-action-container {
                display: flex;
                gap: 2rem;
                align-items: baseline;
            }
        `,
    ];

    connectedCallback() {
        super.connectedCallback();

        this.shadowRoot!.appendChild(this.logActionContainer);
        if (this.logControl) {
            this.logActionContainer.appendChild(this.logControl);
        }
        this.logActionContainer.appendChild(this.logViewActions);

        this.shadowRoot!.appendChild(this.logListContainer);
        this.logListContainer.appendChild(this.logList);
    }

    public updateMonotonicTime(time: number) {
        this.logList.updateTimestamp(time);
    }

    /**
     * Appends a log to the current list of logs
     */
    public addLog(log: LogRowData) {
        this.logList.addLog(log);
    }

    /**
     * Resets the state and contents of the webview to contain nothing.
     * The persistent state is maintained as that represents user selections.
     */
    public reset() {
        this.state.reset();
        this.logList.reset();
        this.logControl?.reset();
    }

    /**
     * Handles a play/pause event emitted by the log actions view.
     * @param playActive whether or not the streaming of logs is active
     */
    private onPlayPauseChangeEvent(playActive: boolean) {
        const detail: ExternalActionRequestEvent = playActive
            ? { type: "resume-log-streaming" }
            : { type: "pause-log-streaming" };
        this.dispatchEvent(
            new CustomEvent(FuchsiaLogViewer.externalActionRequestEvent, {
                detail,
            })
        );

        let logRowData: LogRowData[] = [
            {
                fields: [
                    {
                        key: "timestamp",
                        text: Date.now().toString(),
                        dataFields: [
                            { key: "timestamp", value: Date.now().toString() },
                        ],
                    },
                    {
                        key: "pid",
                        text: "1",
                        dataFields: [{ key: "pid", value: 1 }],
                    },
                    {
                        key: "tid",
                        text: "10",
                        dataFields: [{ key: "tid", value: 10 }],
                    },
                    {
                        key: "moniker",
                        text: "foo",
                        dataFields: [{ key: "moniker", value: "foo" }],
                    },
                    {
                        key: "tags",
                        text: "my-tag",
                        dataFields: [{ key: "tags", value: "my-tag-1" }],
                    },
                    {
                        key: "severity",
                        text: "ERROR",
                        dataFields: [{ key: "severity", value: "error" }],
                    },
                    {
                        key: "message",
                        text: 'An error occurred while processing a request to the /api/v1/users endpoint. The error was: "The user does not exist." A debug message was generated to show the current state of the application.',
                    },
                ],
            },
            {
                fields: [
                    {
                        key: "timestamp",
                        text: Date.now().toString(),
                        dataFields: [
                            { key: "timestamp", value: Date.now().toString() },
                        ],
                    },
                    {
                        key: "pid",
                        text: "2",
                        dataFields: [{ key: "pid", value: 2 }],
                    },
                    {
                        key: "tid",
                        text: "20",
                        dataFields: [{ key: "tid", value: 20 }],
                    },
                    {
                        key: "moniker",
                        text: "foo",
                        dataFields: [{ key: "moniker", value: "foo" }],
                    },
                    {
                        key: "tags",
                        text: "my-tag",
                        dataFields: [{ key: "tags", value: "my-tag-2" }],
                    },
                    {
                        key: "severity",
                        text: "WARNING",
                        dataFields: [{ key: "severity", value: "warning" }],
                    },
                    {
                        key: "message",
                        text: "A warning message was generated because the database connection pool is running low on connections.",
                    },
                ],
                timestamp: Date.now(),
            },
            {
                fields: [
                    {
                        key: "timestamp",
                        text: Date.now().toString(),
                        dataFields: [
                            { key: "timestamp", value: Date.now().toString() },
                        ],
                    },
                    {
                        key: "pid",
                        text: "3",
                        dataFields: [{ key: "pid", value: 3 }],
                    },
                    {
                        key: "tid",
                        text: "30",
                        dataFields: [{ key: "tid", value: 30 }],
                    },
                    {
                        key: "moniker",
                        text: "bar",
                        dataFields: [{ key: "moniker", value: "bar" }],
                    },
                    {
                        key: "tags",
                        text: "my-tag",
                        dataFields: [{ key: "tags", value: "my-tag-3" }],
                    },
                    {
                        key: "severity",
                        text: "INFO",
                        dataFields: [{ key: "severity", value: "info" }],
                    },
                    { key: "message", text: "User 123456 logged in" },
                ],
                timestamp: Date.now(),
            },
        ];

        const randomIndex = Math.floor(Math.random() * logRowData.length);
        logRowData = [logRowData[randomIndex]];

        this.logList.addLog(logRowData[0]);
    }

    //   render() {
    //     return html`<fuchsia-log-view
    //       .headerFields=${this.state.currentFields}
    //     ></fuchsia-log-view>`;
    //   }
}

declare global {
    interface HTMLElementTagNameMap {
        "fuchsia-log-viewer": FuchsiaLogViewer;
    }
}
