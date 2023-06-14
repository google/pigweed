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

import { LitElement, html } from "lit";
import { customElement, property, state } from "lit/decorators.js";
import { styles } from "./log-view.styles";
import { LogEntry } from "../../shared/interfaces";


// Import subcomponents
import "./log-list/log-list";
import "./controls/controls";

/**
 * Description of LogView.
 */
@customElement("log-view")
export class LogView extends LitElement {
    static styles = styles;

    /**
     * Description of logs.
     */
    @property({ type: Array })
    logs: LogEntry[] = [];

    /**
     * Logs filtered based on the user filter
     */
    @property({ type: Array })
    _filteredLogs: LogEntry[] = [];

    /**
     * Fields from some log source
     */
    @property({ type: Array, reflect: true })
    _fields: String[] = [];

    /**
     * Description of selectedHostId.
     */
    @state()
    private _selectedHostId: string = 'example-host';

    /**
     * Description of filter.
     */
    @state()
    private _filter: (logEntry: LogEntry) => boolean;

    constructor() {
        super();
        this._filter = (logEntry) => logEntry.hostId === this._selectedHostId;
    }

    render() {
        this._filteredLogs = JSON.parse(JSON.stringify(this.logs.filter(this._filter)));
        this._fields = this.getFields(this.logs);
        return html`<log-view-controls .fieldKeys=${this._fields} 
        @field-toggle="${this.handleFieldToggleEvent}"role="toolbar"></log-view-controls>
                <log-list .logs=${this._filteredLogs}></log-list>`;
    }

    /**
     * Assigns logFields based on the log source
     * @param logs the source logs to extract fields from
     * @return     an array of log fields
     */
    private getFields(logs: LogEntry[]): String[] {
        const log = logs[0];
        const logFields = [] as String[];
        if (log != undefined) {
            log.fields.forEach((field) => {
                logFields.push(field.key);
            });
        }
        return logFields;
    }

    /**
     * Handles the fieldToggle event for field visibility
     * @param e click event from the toggled field
     */
    handleFieldToggleEvent(e: CustomEvent) {
        // should be index to show/hide element
        let index = -1;

        // TODO(b/287285444): surface table element as property of LogList
        const logListEl = this.renderRoot.querySelector('log-list') as HTMLElement;
        const tableBodyEl = logListEl?.shadowRoot?.querySelector('tbody') as HTMLElement;
        const table = Array.from(tableBodyEl?.children) as HTMLElement[];

        // Get index from the source logs to show field back into view
        if (e.detail.isChecked) {
            this._fields.forEach((_, i: number) => {
                if (this.logs[0].fields[i].key == e.detail.field) {
                    index = i;
                }
            });

            table.forEach((_, i: number) => {
                let tableCellEl = table[i].children[index] as HTMLElement;
                tableCellEl.hidden = false;
            });
        }

        // Get index from dropdown and hide field from view
        else {
            this._fields.forEach((_, i: number) => {
                if (this._filteredLogs[0].fields[i].key === e.detail.field) {
                    index = i;
                }
            });

            table.forEach((_, i: number) => {
                let tableCellEl = table[i].children[index] as HTMLElement;
                tableCellEl.hidden = true;
            });
        }
    }
}

