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

import { LitElement, PropertyValues, html } from 'lit';
import { customElement, property, query, state } from 'lit/decorators.js';
import { styles } from './log-view.styles';
import { LogList } from '../log-list/log-list';
import { FieldData, LogEntry } from '../../shared/interfaces';
import '../log-list/log-list';
import '../log-view-controls/log-view-controls';

/** Used for generating a unique id for the view. */
let viewCount = 0;

type LogFilter = (logEntry: LogEntry) => boolean;

/**
 * A component that filters and displays incoming log entries in an encapsulated
 * instance. Each `LogView` contains a log list and a set of log view controls
 * for configurable viewing of filtered logs.
 *
 * @element log-view
 */
@customElement('log-view')
export class LogView extends LitElement {
    static styles = styles;

    /**
     * The component's global `id` attribute. This unique value is set whenever
     * a view is created in a log viewer instance.
     */
    @property({ type: String })
    id = `${this.localName}-${viewCount}`;

    /** An array of log entries to be displayed. */
    @property({ type: Array })
    logs: LogEntry[] = [];

    /** Indicates whether this view is one of multiple instances. */
    @property({ type: Boolean })
    isOneOfMany = false;

    /**
     * An array containing the logs that remain after the current filter has
     * been applied.
     */
    @state()
    private _filteredLogs: LogEntry[] = [];

    /** The field keys (column values) for the incoming log entries. */
    @state()
    private _fieldKeys: string[] = [];

    /** A function used for filtering rows that contain a certain substring. */
    @state()
    private _stringFilter: LogFilter = () => true;

    /**
     * A function used for filtering rows that contain a timestamp within a
     * certain window.
     */
    @state()
    private _timeFilter: LogFilter = () => true;

    /** A string representing the value contained in the search field. */
    @state()
    private _searchText = '';

    @query('log-list') _logList!: LogList;

    connectedCallback() {
        super.connectedCallback();
        viewCount++;
    }

    updated(changedProperties: PropertyValues) {
        super.updated(changedProperties);

        if (changedProperties.has('logs')) {
            this._fieldKeys = this.getFieldsFromLogs(this.logs);
            this.filterLogs();
        }
    }

    /**
     * Updates the log filter based on the provided event type.
     *
     * @param {CustomEvent} event - The custom event containing the information
     *   to update the filter.
     */
    private updateFilter(event: CustomEvent) {
        switch (event.type) {
            case 'input-change':
                this._searchText = event.detail.inputValue;

                if (this._searchText) {
                    this._stringFilter = (logEntry: LogEntry) =>
                        logEntry.fields
                            .filter(
                                // Exclude severity field, since its text is omitted from the table
                                (field: FieldData) => field.key !== 'severity'
                            )
                            .some((field: FieldData) =>
                                new RegExp(this._searchText, 'i').test(
                                    field.value.toString()
                                )
                            );
                } else {
                    this._stringFilter = () => true;
                }
                break;
            case 'clear-logs':
                this._timeFilter = (logEntry) =>
                    logEntry.timestamp > event.detail.timestamp;
                break;
            default:
                break;
        }

        this.filterLogs();
        this.requestUpdate();
    }

    /**
     * Retrieves the field keys from the first entry in the log array.
     *
     * @param {LogEntry[]} logs - The array of log entries from which to
     *   retrieve the field keys.
     * @returns {string[]} An array containing the field keys from the log
     *   entries.
     */
    private getFieldsFromLogs(logs: LogEntry[]): string[] {
        const logEntry = logs[0];
        const logFields = [] as string[];

        if (logEntry != undefined) {
            logEntry.fields.forEach((field) => {
                logFields.push(field.key);
            });
        }

        return logFields.filter((field) => field !== 'severity');
    }

    /**
     * Toggles the visibility of columns in the log list based on the provided
     * event.
     *
     * @param {CustomEvent} event - The click event containing the field being
     *   toggled.
     */
    private toggleColumns(event: CustomEvent) {
        const colsHidden = this._logList.colsHidden;
        let index = -1;

        this._fieldKeys.forEach((field: string, i: number) => {
            if (field == event.detail.field) {
                index = i;
            }
        });

        colsHidden[index + 1] = !event.detail.isChecked; // Exclude first column (severity)
        this._logList.colsHidden = [...colsHidden];
    }

    /**
     * Combines constituent filter expressions and filters the logs. The
     * filtered logs are stored in the `_filteredLogs` state property.
     */
    private filterLogs() {
        const combinedFilter = (logEntry: LogEntry) =>
            this._timeFilter(logEntry) && this._stringFilter(logEntry);

        this._filteredLogs = JSON.parse(
            JSON.stringify(this.logs.filter(combinedFilter))
        );
    }

    render() {
        return html` <log-view-controls
                .viewId=${this.id}
                .fieldKeys=${this._fieldKeys}
                .hideCloseButton=${!this.isOneOfMany}
                @input-change="${this.updateFilter}"
                @clear-logs="${this.updateFilter}"
                @column-toggle="${this.toggleColumns}"
                role="toolbar"
            >
            </log-view-controls>

            <log-list
                .viewId=${this.id}
                .logs=${this._filteredLogs}
                .searchText=${this._searchText}
            ></log-list>`;
    }
}
