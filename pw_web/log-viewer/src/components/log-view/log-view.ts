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
import { LogColumnState, LogEntry, State } from '../../shared/interfaces';
import { LocalStorageState, StateStore } from '../../shared/state';
import { LogFilter } from '../../utils/log-filter/log-filter';
import '../log-list/log-list';
import '../log-view-controls/log-view-controls';

type FilterFunction = (logEntry: LogEntry) => boolean;

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

  /** Whether line wrapping in table cells should be used. */
  @state()
  _lineWrap = false;

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
  private _stringFilter: FilterFunction = () => true;

  /**
   * A function used for filtering rows that contain a timestamp within a
   * certain window.
   */
  @state()
  private _timeFilter: FilterFunction = () => true;

  /** A string representing the value contained in the search field. */
  @state()
  public searchText = '';

  /** A StateStore object that stores state of views */
  @state()
  _stateStore: StateStore = new LocalStorageState();

  @state()
  _state: State;

  @state()
  _colsHidden: (boolean | undefined)[] = [];

  @query('log-list') _logList!: LogList;

  private _debounceTimeout: NodeJS.Timeout | null = null;

  /** The amount of time, in ms, before the filter expression is executed. */
  private readonly FILTER_DELAY = 100;

  constructor() {
    super();
    this._state = this._stateStore.getState();
  }

  protected firstUpdated(): void {
    this._colsHidden = [];

    if (this._state) {
      const viewConfigArr = this._state.logViewConfig;
      const index = viewConfigArr.findIndex((i) => this.id === i.viewID);

      if (index !== -1) {
        viewConfigArr[index].search = this.searchText;
        viewConfigArr[index].columns.map((i: LogColumnState) => {
          this._colsHidden.push(i.hidden);
        });
        this._colsHidden.unshift(undefined);
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
    this.searchText = event.detail.inputValue;
    const viewConfigArr = this._state.logViewConfig;
    const index = viewConfigArr.findIndex((i) => this.id === i.viewID);

    switch (event.type) {
      case 'input-change':
        if (this._debounceTimeout) {
          clearTimeout(this._debounceTimeout);
        }

        if (index !== -1) {
          viewConfigArr[index].search = this.searchText;
          this._state = { logViewConfig: viewConfigArr };
          this._stateStore.setState({ logViewConfig: viewConfigArr });
        }

        if (!this.searchText) {
          this._stringFilter = () => true;
          return;
        }

        // Run the filter after the timeout delay
        this._debounceTimeout = setTimeout(() => {
          const filters = LogFilter.parseSearchQuery(this.searchText).map(
            (condition) => LogFilter.createFilterFunction(condition),
          );
          this._stringFilter =
            filters.length > 0
              ? (logEntry: LogEntry) =>
                  filters.some((filter) => filter(logEntry))
              : () => true;

          this.filterLogs();
          this.requestUpdate();
        }, this.FILTER_DELAY);
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
    const viewConfigArr = this._state.logViewConfig;
    let colIndex = -1;

    this._colsHidden = [];
    const index = viewConfigArr
      .map((i) => {
        return i.viewID;
      })
      .indexOf(this.id);
    viewConfigArr[index].columns.map((i: LogColumnState) => {
      this._colsHidden.push(i.hidden);
    });
    this._colsHidden.unshift(undefined);

    this._fieldKeys.forEach((field: string, i: number) => {
      if (field == event.detail.field) {
        colIndex = i;
        viewConfigArr[index].columns[colIndex].hidden = !event.detail.isChecked;
      }
    });

    this._colsHidden[colIndex + 1] = !event.detail.isChecked; // Exclude first column (severity)
    this._logList.colsHidden = [...this._colsHidden];

    this._state = { logViewConfig: viewConfigArr };
    this._stateStore.setState({ logViewConfig: viewConfigArr });
  }

  /**
   * Toggles the wrapping of text in each row.
   *
   * @param {CustomEvent} event - The click event.
   */
  private toggleWrapping() {
    this._lineWrap = !this._lineWrap;
  }

  /**
   * Combines constituent filter expressions and filters the logs. The
   * filtered logs are stored in the `_filteredLogs` state property.
   */
  private filterLogs() {
    const combinedFilter = (logEntry: LogEntry) =>
      this._timeFilter(logEntry) && this._stringFilter(logEntry);

    this._filteredLogs = JSON.parse(
      JSON.stringify(this.logs.filter(combinedFilter)),
    );
  }

  render() {
    return html` <log-view-controls
        .colsHidden=${[...this._colsHidden]}
        .viewId=${this.id}
        .fieldKeys=${this._fieldKeys}
        .hideCloseButton=${!this.isOneOfMany}
        .stateStore=${this._stateStore}
        @input-change="${this.updateFilter}"
        @clear-logs="${this.updateFilter}"
        @column-toggle="${this.toggleColumns}"
        @wrap-toggle="${this.toggleWrapping}"
        role="toolbar"
      >
      </log-view-controls>

      <log-list
        .colsHidden=${[...this._colsHidden]}
        .lineWrap=${this._lineWrap}
        .viewId=${this.id}
        .logs=${this._filteredLogs}
        .searchText=${this.searchText}
      >
      </log-list>`;
  }
}
