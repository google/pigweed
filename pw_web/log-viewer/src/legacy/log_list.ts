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

import { html, LitElement, css } from "lit";
import { customElement, state } from "lit/decorators.js";
import * as constant from "./models/constants";
import { LogHeader } from "./log_header";
import { LogRow } from "./log_row";
import { LogRowData } from "./models/log_data";
import {
    LogField,
    LogHeadersData,
    LogViewOptions,
    HeaderWidthChangeHandler,
} from "./models/fields";
import { Filter, FilterExpression, NoTimestampError } from "./models/filter";
// import {ExternalActionRequestEvent, LogViewer} from '../log-viewer';

export const WRAP_LOG_TEXT_ATTR = "wrap-log-text";

const logStyles = css`
    log-header > div {
        background-color: var(--vscode-editor-background);
        border-bottom: 2px solid var(--vscode-panel-border);
        padding-bottom: 5px;
        position: sticky;
        top: 0;
    }

    log-header,
    log-view-row {
        display: table-row-group;
    }

    log-view-row[hidden] {
        display: none;
    }

    log-view-row > div {
        display: table-row;
    }

    .log-list-cell,
    log-header > div {
        overflow: clip;
        text-align: left;
        text-overflow: ellipsis;
        vertical-align: top;
        white-space: nowrap;
        display: table-cell;
        padding: 0.5rem 0;
    }

    td#moniker {
        direction: rtl;
    }

    :host([wrap-log-text]) .msg-cell,
    :host(:not([wrap-log-text])) .msg-cell:hover {
        text-overflow: clip;
        white-space: normal;
    }

    :host(:not([wrap-log-text])) .msg-cell:hover {
        background: var(--vscode-list-hoverBackground);
    }

    :host {
        display: table;
        table-layout: fixed;
        width: 100%;
        word-wrap: normal;
    }

    log-view-row {
        font-family: var(--vscode-editor-font-family);
        font-size: var(--vscode-editor-font-size);
        font-weight: var(--vscode-editor-font-weight);
        font-family: "Roboto Mono";
        font-size: 1rem;
    }

    log-view-row div[data-severity="warn"] {
        color: var(--vscode-list-warningForeground);
    }

    log-view-row div[data-severity="error"] {
        color: var(--vscode-list-errorForeground);
    }

    log-view-row div[data-moniker="<VSCode>"] {
        /* line-height: 75%; */
        opacity: 70%;
    }

    .column-resize {
        cursor: col-resize;
        height: 100%;
        position: absolute;
        right: 0;
        top: 0;
        width: 5px;
    }

    .column-resize:hover,
    .resizing {
        border-right: 2px solid var(--vscode-editor-foreground);
    }
`;

@customElement("fuchsia-log-view")
export class LogList extends LitElement {
    static styles = [logStyles];

    @state()
    private logRows: Array<LitElement> = [];
    private maxWidths: Array<number>;
    private logHeader: LogHeader;
    public lastTimestamp = 0;
    public filterResultsCount = 0;
    private pendingFilterChange?: CustomEvent;
    private pendingFilter?: Filter;

    constructor(
        private headerFields: LogHeadersData,
        shouldWrapLogs: boolean,
        onHeaderWidthChange: HeaderWidthChangeHandler,
        private currentFilter: FilterExpression,
        private options: LogViewOptions
    ) {
        super();
        this.maxWidths = new Array(Object.keys(headerFields).length).fill(0);
        this.logHeader = new LogHeader(headerFields, onHeaderWidthChange);
        this.logWrapping = shouldWrapLogs;
        this.ariaColCount = `${Object.keys(headerFields).length}`;
    }

    public updateTimestamp(timestamp: number) {
        this.pendingFilter!.nowTimestampFromDevice = timestamp;
        this.filterChangeHandler(this.pendingFilterChange!);
    }

    // Retrieves the current monotonic time from ffx
    // This is needed so that when the user presses enter
    // we fetch the most recent timestamp from the device to compare against.
    //   fetchTimestamp(event: CustomEvent, er: NoTimestampError) {
    fetchTimestamp() {
        // this.pendingFilter = er.filter;
        // this.pendingFilterChange = event;
        // const detail: ExternalActionRequestEvent = {type: 'fetch-clock-monotonic'};
        // this.dispatchEvent(
        //   new CustomEvent(LogViewer.externalActionRequestEvent, {detail})
        // );
        return Date.now();
    }

    filterChangeHandler(event: CustomEvent) {
        const filter: FilterExpression = event.detail.filter;
        this.ariaColCount = `${this.logRows.length}`;
        this.currentFilter = filter;
        let resultCount = 0;

        if (filter.isEmpty()) {
            for (const element of this.logRows) {
                element.removeAttribute("hidden");
                resultCount++;
            }
        } else {
            try {
                for (const element of this.logRows) {
                    if (filter.accepts(element.children[0])) {
                        element.removeAttribute("hidden");
                        resultCount++;
                    } else {
                        element.setAttribute("hidden", "");
                    }
                }
            } catch (er) {
                if (er instanceof NoTimestampError) {
                    //   this.fetchTimestamp(event, er);
                    this.fetchTimestamp();
                } else {
                    throw er;
                }
            }
        }

        this.filterResultsCount = resultCount;
    }

    render() {
        return html` ${this.logHeader} ${this.logRows} `;
    }

    /**
     * Defines whether or not to wrap the log text.
     *
     * @param logWrapActive if true the log text will be wrapped
     */
    public set logWrapping(logWrapActive: boolean) {
        if (logWrapActive) {
            this.setAttribute(WRAP_LOG_TEXT_ATTR, "true");
        } else {
            this.removeAttribute(WRAP_LOG_TEXT_ATTR);
        }
    }

    /**
     * Appends a log to the current list of logs
     */
    public addLog(log: LogRowData) {
        const timestampField = log.timestamp;
        // Keep the timestamp around so we know when to start of from
        // when we clear logs.
        if (timestampField && timestampField > this.lastTimestamp) {
            // ffx does not guarantee that logs come in order
            // of timestamp.
            this.lastTimestamp = timestampField;
        }
        this.addLogElement(log);
    }

    /**
     * Resets the state and contents of the webview to contain nothing.
     * The persistent state is maintained as that represents user selections.
     */
    public reset() {
        this.logRows = [];
        this.filterResultsCount = 0;
    }

    private addLogElement(log: LogRowData) {
        const logsToDrop = this.logRows.length - constant.MAX_LOGS;
        for (let i = 0; i < logsToDrop; i++) {
            this.popLogElement();
        }
        this.appendLogElement(log);
    }

    private appendLogElement(log: LogRowData) {
        const element = new LogRow(
            log,
            this.headerFields,
            this.options.columnFormatter
        );
        this.logRows = [...this.logRows, element];
        element.updateComplete
            .then(() => {
                if (!this.filtersAllow(element.children[0])) {
                    element.setAttribute("hidden", "");
                }
                this.setMaxCellWidth(element.children[0]);
                if (!this.parentElement?.matches(":hover")) {
                    this.parentElement?.scrollTo(-1, this.scrollHeight);
                }
            })
            .catch(() => {});
    }

    private setMaxCellWidth(row: Element) {
        for (const cell in Array.from(row.children)) {
            const id = row.children[cell].id as LogField;
            const width = this.maxWidths[cell];
            if (width < row.children[cell].scrollWidth) {
                this.maxWidths[cell] = row.children[cell].scrollWidth;
                this.logHeader.setMaxCellWidth(id, this.maxWidths[cell]);
            }
        }
    }

    private popLogElement() {
        this.logRows = this.logRows.slice(1);
    }

    private filtersAllow(el: Element) {
        return this.currentFilter.accepts(el);
    }
}
