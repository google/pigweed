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
    logs: LogEntry[];

    /**
     * Description of selectedHostId.
     */
    @state()
    private _selectedHostId: string;

    /**
     * Description of filter.
     */
    @state()
    private _filter: (logEntry: LogEntry) => boolean;

    constructor() {
        super();
        this.logs = [];
        this._selectedHostId = "example-host";
        this._filter = (logEntry) => logEntry.hostId === this._selectedHostId;
    }

    render() {
        const filteredLogs = this.logs.filter(this._filter);

        return html` <log-view-controls role="toolbar"></log-view-controls>
            <log-list .logs=${filteredLogs}></log-list>`;
    }
}
