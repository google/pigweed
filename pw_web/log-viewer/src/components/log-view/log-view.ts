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

import { LitElement, html } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';
import { styles } from './log-view.styles';
import { LogEntry } from '../../shared/interfaces';

// Import subcomponents
import './log-list/log-list';
import './controls/controls';
import { LogList } from './log-list/log-list';

interface FilterChangeEvent extends Event {
    detail: {
        filterValue: string;
    };
}

declare global {
    interface HTMLElementEventMap {
        'filter-change': FilterChangeEvent;
    }
}

let viewCount = 0;

/**
 * Description of LogView.
 */
@customElement('log-view')
export class LogView extends LitElement {
    static styles = styles;

    /**
     * Description of id.
     */
    @property({ type: String })
    id: string;

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
    @property({ type: Array })
    _fields: string[] = [];

    /**
     * Description of filter.
     */
    @state()
    private _filter: (logEntry: LogEntry) => boolean = () => true;

    /**
     * Description of filterValue.
     */
    @state()
    private _filterValue = '';

    /**
     * Description of hideCloseButton.
     */
    @property({ type: Boolean })
    hideCloseButton = false;

    constructor() {
        super();
        this.id = `log-view-${viewCount}`;
    }

    connectedCallback() {
        super.connectedCallback();
        viewCount++;
        this.addEventListener('filter-change', this.handleFilterChange);
        this.addEventListener('clear-logs', this.handleClearLogs);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        this.removeEventListener('filter-change', this.handleFilterChange);
        this.removeEventListener('clear-logs', this.handleClearLogs);
    }

    handleFilterChange(event: FilterChangeEvent) {
        this._filterValue = event.detail.filterValue;
        const regex = new RegExp(this._filterValue, 'i');

        this._filter = (logEntry) =>
            logEntry.fields.some((field) => regex.test(field.value.toString()));
        this.requestUpdate();
    }

    handleClearLogs() {
        const timeThreshold = new Date();
        const existingFilter = this._filter;
        this._filter = (logEntry) => {
            return (
                existingFilter(logEntry) && logEntry.timestamp > timeThreshold
            );
        };
    }

    /**
     * Assigns logFields based on the log source
     * @param logs the source logs to extract fields from
     * @return     an array of log fields
     */
    private getFields(logs: LogEntry[]): string[] {
        const log = logs[0];
        const logFields = [] as string[];
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

        const logList = this.renderRoot.querySelector('log-list') as LogList;
        const colsHidden = logList.colsHidden;

        this._fields.forEach((_, i: number) => {
            if (this.logs[0].fields[i].key == e.detail.field) {
                index = i;
            }
        });

        colsHidden[index] = !e.detail.isChecked;

        logList.colsHidden = colsHidden;
        logList.clearGridTemplateColumns();
        logList.updateGridTemplateColumns();
    }

    render() {
        this._filteredLogs = JSON.parse(
            JSON.stringify(this.logs.filter(this._filter))
        );
        this._fields = this.getFields(this.logs);

        const passedFilterValue = this._filterValue;

        return html` <log-view-controls
                .viewId=${this.id}
                .fieldKeys=${this._fields}
                .hideCloseButton=${this.hideCloseButton}
                @field-toggle="${this.handleFieldToggleEvent}"
                role="toolbar"
            >
            </log-view-controls>

            <log-list
                .viewId=${this.id}
                .logs=${this._filteredLogs}
                .filterValue=${passedFilterValue}
            ></log-list>`;
    }
}
