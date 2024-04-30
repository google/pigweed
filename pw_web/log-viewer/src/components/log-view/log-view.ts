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
import {
  TableColumn,
  LogEntry,
  State,
  SourceData,
} from '../../shared/interfaces';
import { LocalStorageState, StateStore } from '../../shared/state';
import { LogFilter } from '../../utils/log-filter/log-filter';
import '../log-list/log-list';
import '../log-view-controls/log-view-controls';
import { downloadTextLogs } from '../../utils/download';

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
   * The component's global `id` attribute. This unique value is set whenever a
   * view is created in a log viewer instance.
   */
  @property({ type: String })
  id = `${this.localName}-${crypto.randomUUID()}`;

  /** An array of log entries to be displayed. */
  @property({ type: Array })
  logs: LogEntry[] = [];

  /** Indicates whether this view is one of multiple instances. */
  @property({ type: Boolean })
  isOneOfMany = false;

  /** The title of the log view, to be displayed on the log view toolbar */
  @property({ type: String })
  viewTitle = '';

  /** Whether line wrapping in table cells should be used. */
  @state()
  _lineWrap = false;

  /** The field keys (column values) for the incoming log entries. */
  @state()
  public columnData: TableColumn[] = [];

  /** A string representing the value contained in the search field. */
  @state()
  public searchText = '';

  /** A StateStore object that stores state of views */
  @state()
  _stateStore: StateStore = new LocalStorageState();

  /** Preferred column order to reference */
  @state()
  columnOrder: string[] = [];

  @query('log-list') _logList!: LogList;

  /** A map containing data from present log sources */
  sources: Map<string, SourceData> = new Map();

  /**
   * An array containing the logs that remain after the current filter has been
   * applied.
   */
  private _filteredLogs: LogEntry[] = [];

  /** A function used for filtering rows that contain a certain substring. */
  private _stringFilter: FilterFunction = () => true;

  /**
   * A function used for filtering rows that contain a timestamp within a
   * certain window.
   */
  private _timeFilter: FilterFunction = () => true;

  private _state: State;

  private _debounceTimeout: NodeJS.Timeout | null = null;

  /** The number of elements in the `logs` array since last updated. */
  private _lastKnownLogLength: number = 0;

  /** The amount of time, in ms, before the filter expression is executed. */
  private readonly FILTER_DELAY = 100;

  constructor() {
    super();
    this._state = this._stateStore.getState();
  }

  protected firstUpdated(): void {
    const viewConfigArr = this._state.logViewConfig;
    const index = viewConfigArr.findIndex((i) => this.id === i.viewID);

    // Get column data from local storage, if it exists
    if (index !== -1) {
      const storedColumnData = viewConfigArr[index].columnData;
      this.updateColumnOrder(storedColumnData);
      this.columnData = this.updateColumnRender(storedColumnData);
    }

    // Update view title with log source names if a view title isn't already provided
    if (!this.viewTitle) {
      this.updateTitle();
    }
  }

  updated(changedProperties: PropertyValues) {
    super.updated(changedProperties);

    if (changedProperties.has('logs')) {
      const newLogs = this.logs.slice(this._lastKnownLogLength);
      this._lastKnownLogLength = this.logs.length;

      this.updateFieldsFromNewLogs(newLogs);
      this.updateTitle();
    }

    if (changedProperties.has('logs') || changedProperties.has('searchText')) {
      this.filterLogs();
    }

    if (changedProperties.has('_columnData')) {
      this._state = { logViewConfig: this._state.logViewConfig };
      this._stateStore.setState({
        logViewConfig: this._state.logViewConfig,
      });
    }
  }

  /**
   * Updates the log filter based on the provided event type.
   *
   * @param {CustomEvent} event - The custom event containing the information to
   *   update the filter.
   */
  private updateFilter(event: CustomEvent) {
    this.searchText = event.detail.inputValue;
    const logViewConfig = this._state.logViewConfig;
    const index = logViewConfig.findIndex((i) => this.id === i.viewID);

    switch (event.type) {
      case 'input-change':
        if (this._debounceTimeout) {
          clearTimeout(this._debounceTimeout);
        }

        if (index !== -1) {
          logViewConfig[index].search = this.searchText;
          this._state = { logViewConfig: logViewConfig };
          this._stateStore.setState({ logViewConfig: logViewConfig });
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

  private updateFieldsFromNewLogs(newLogs: LogEntry[]): void {
    if (!this.columnData) {
      this.columnData = [];
    }

    newLogs.forEach((log) => {
      log.fields.forEach((field) => {
        if (!this.columnData.some((col) => col.fieldName === field.key)) {
          const newColumnData = {
            fieldName: field.key,
            characterLength: 0,
            manualWidth: null,
            isVisible: true,
          };
          this.updateColumnOrder([newColumnData]);
          this.columnData = this.updateColumnRender([
            newColumnData,
            ...this.columnData,
          ]);
        }
      });
    });
  }

  /**
   * Orders fields by the following: severity, init defined fields, undefined fields, and message
   * @param columnData ColumnData is used to check for undefined fields.
   */
  private updateColumnOrder(columnData: TableColumn[]) {
    const columnOrder = [...new Set(this.columnOrder)];
    if (this.columnOrder.length !== columnOrder.length) {
      console.warn(
        'Log View had duplicate columns defined, duplicates were removed.',
      );
      this.columnOrder = columnOrder;
    }

    if (this.columnOrder.indexOf('severity') != 0) {
      const index = this.columnOrder.indexOf('severity');
      if (index != -1) {
        this.columnOrder.splice(index, 1);
      }
      this.columnOrder.unshift('severity');
    }

    if (this.columnOrder.indexOf('message') != this.columnOrder.length) {
      const index = this.columnOrder.indexOf('message');
      if (index != -1) {
        this.columnOrder.splice(index, 1);
      }
      this.columnOrder.push('message');
    }

    columnData.forEach((tableColumn) => {
      if (!this.columnOrder.includes(tableColumn.fieldName)) {
        this.columnOrder.splice(
          this.columnOrder.length - 1,
          0,
          tableColumn.fieldName,
        );
      }
    });
  }

  /**
   * Updates order of columnData based on columnOrder for log viewer to render
   * @param columnData ColumnData to order
   * @return Ordered list of ColumnData
   */
  private updateColumnRender(columnData: TableColumn[]): TableColumn[] {
    const orderedColumns: TableColumn[] = [];
    const columnFields = columnData.map((column) => {
      return column.fieldName;
    });

    this.columnOrder.forEach((field: string) => {
      const index = columnFields.indexOf(field);
      if (index > -1) {
        orderedColumns.push(columnData[index]);
      }
    });

    return orderedColumns;
  }

  public getFields(): string[] {
    return this.columnData
      .filter((column) => column.isVisible)
      .map((column) => column.fieldName);
  }

  /**
   * Toggles the visibility of columns in the log list based on the provided
   * event.
   *
   * @param {CustomEvent} event - The click event containing the field being
   *   toggled.
   */
  private toggleColumns(event: CustomEvent) {
    const logViewConfig = this._state.logViewConfig;
    const index = logViewConfig.findIndex((i) => this.id === i.viewID);

    if (index === -1) {
      return;
    }

    // Find the relevant column in _columnData
    const column = this.columnData.find(
      (col) => col.fieldName === event.detail.field,
    );

    if (!column) {
      return;
    }

    // Toggle the column's visibility
    column.isVisible = event.detail.isChecked;

    // Clear the manually-set width of the last visible column
    const lastVisibleColumn = this.columnData
      .slice()
      .reverse()
      .find((col) => col.isVisible);
    if (lastVisibleColumn) {
      lastVisibleColumn.manualWidth = null;
    }

    // Trigger the change in column data and request an update
    this.columnData = [...this.columnData];
    this._logList.requestUpdate();
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
   * Combines filter expressions and filters the logs. The filtered
   * logs are stored in the `_filteredLogs` property.
   */
  private filterLogs() {
    const combinedFilter = (logEntry: LogEntry) =>
      this._timeFilter(logEntry) && this._stringFilter(logEntry);

    const newFilteredLogs = this.logs.filter(combinedFilter);

    if (
      JSON.stringify(newFilteredLogs) !== JSON.stringify(this._filteredLogs)
    ) {
      this._filteredLogs = newFilteredLogs;
    }
  }

  private updateColumnData(event: CustomEvent) {
    this.columnData = event.detail;
  }

  private updateTitle() {
    const sourceNames = Array.from(this.sources.values())?.map(
      (tag: SourceData) => tag.name,
    );
    this.viewTitle = sourceNames.join(', ');
  }

  /**
   * Generates a log file in the specified format and initiates its download.
   *
   * @param {CustomEvent} event - The click event.
   */
  private downloadLogs(event: CustomEvent) {
    const headers = this.columnData.map((column) => column.fieldName);
    const viewTitle = event.detail.viewTitle;
    downloadTextLogs(this.logs, headers, viewTitle);
  }

  render() {
    return html` <log-view-controls
        .columnData=${this.columnData}
        .viewId=${this.id}
        .viewTitle=${this.viewTitle}
        .hideCloseButton=${!this.isOneOfMany}
        .stateStore=${this._stateStore}
        @input-change="${this.updateFilter}"
        @clear-logs="${this.updateFilter}"
        @column-toggle="${this.toggleColumns}"
        @wrap-toggle="${this.toggleWrapping}"
        @download-logs="${this.downloadLogs}"
        role="toolbar"
      >
      </log-view-controls>

      <log-list
        .columnData=${[...this.columnData]}
        .lineWrap=${this._lineWrap}
        .viewId=${this.id}
        .logs=${this._filteredLogs}
        .searchText=${this.searchText}
        @update-column-data="${this.updateColumnData}"
      >
      </log-list>`;
  }
}
