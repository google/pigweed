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
import { FieldData, LogColumnState, LogEntry, State } from '../../shared/interfaces';
import { LocalStorageState, StateStore } from '../../shared/state';
import '../log-list/log-list';
import '../log-view-controls/log-view-controls';

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
    id = `${this.localName}-${crypto.randomUUID()}`;

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
    public searchText: string = '';

    /** A StateStore object that stores state of views */
    @state()
    _stateStore: StateStore = new LocalStorageState();

    @state()
    _state: State;

    colsHidden: (boolean|undefined)[] = [];

    @query('log-list') _logList!: LogList;

    constructor() {
        super();
        this._state = this._stateStore.getState();
    }

    protected firstUpdated(): void {
        this.colsHidden = [];

        if (this._state) {
            let viewConfigArr = this._state.logViewConfig;
            const index = viewConfigArr.findIndex((i) => this.id === i.viewID);

            if (index !== -1) {
                viewConfigArr[index].search = this.searchText;
                viewConfigArr[index].columns.map((i: LogColumnState) => { this.colsHidden.push(i.hidden) });
                this.colsHidden.unshift(undefined);
            }
        }
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
                this.searchText = event.detail.inputValue;

                if (!this.searchText) {
                    this._stringFilter = () => true;
                    return;
                }

                this._stringFilter = (logEntry: LogEntry) =>
                    logEntry.fields
                        .filter(
                            // Exclude severity field, since its text is omitted from the table
                            (field: FieldData) => field.key !== 'severity'
                        )
                        .some((field: FieldData) =>
                            new RegExp(this.searchText, 'i').test(
                                field.value.toString()
                            )
                        );
                break;
            case 'clear-logs':
                this._timeFilter = (logEntry) =>
                    logEntry.timestamp > event.detail.timestamp;
                break;
            default:
                break;
        }

        let viewConfigArr = this._state.logViewConfig;
        const index = viewConfigArr.findIndex((i) => this.id === i.viewID);
        if (index !== -1) {
            viewConfigArr[index].search = this.searchText;
            this._state = { logViewConfig: viewConfigArr };
            this._stateStore.setState({ logViewConfig: viewConfigArr });
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
    public getFieldsFromLogs(logs: LogEntry[]): string[] {
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
        let viewConfigArr = this._state.logViewConfig;
        let colIndex = -1;

        this.colsHidden = [];
        const index = viewConfigArr.map((i) => { return i.viewID }).indexOf(this.id);
        viewConfigArr[index].columns.map((i: LogColumnState) => { this.colsHidden.push(i.hidden) });
        this.colsHidden.unshift(undefined);

        this._fieldKeys.forEach((field: string, i: number) => {
            if (field == event.detail.field) {
                colIndex = i;
                viewConfigArr[index].columns[colIndex].hidden = !event.detail.isChecked;
            }
        });

        this.colsHidden[colIndex + 1] = !event.detail.isChecked; // Exclude first column (severity)
        this._logList.colsHidden = [...this.colsHidden];

        this._state = { logViewConfig: viewConfigArr };
        this._stateStore.setState({ logViewConfig: viewConfigArr });
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
                .colsHidden=${[...this.colsHidden]}
                .viewId=${this.id}
                .fieldKeys=${this._fieldKeys}
                .hideCloseButton=${!this.isOneOfMany}
                .stateStore=${this._stateStore}
                @input-change="${this.updateFilter}"
                @clear-logs="${this.updateFilter}"
                @column-toggle="${this.toggleColumns}"
                role="toolbar"
            >
            </log-view-controls>

            <log-list
                .colsHidden=${[...this.colsHidden]}
                .viewId=${this.id}
                .logs=${this._filteredLogs}
                .searchText=${this.searchText}
            >
            </log-list>`;
    }
}
