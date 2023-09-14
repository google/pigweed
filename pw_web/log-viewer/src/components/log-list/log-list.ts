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

import { LitElement, html, PropertyValues, TemplateResult } from 'lit';
import {
  customElement,
  property,
  query,
  queryAll,
  state,
} from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';
import { styles } from './log-list.styles';
import { LogEntry, Severity, TableColumn } from '../../shared/interfaces';
import { virtualize } from '@lit-labs/virtualizer/virtualize.js';
import '@lit-labs/virtualizer';

/**
 * A sub-component of the log view which takes filtered logs and renders them in
 * a virtualized HTML table.
 *
 * @element log-list
 */
@customElement('log-list')
export class LogList extends LitElement {
  static styles = styles;

  /** The `id` of the parent view containing this log list. */
  @property()
  viewId = '';

  /** An array of log entries to be displayed. */
  @property({ type: Array })
  logs: LogEntry[] = [];

  /** A string representing the value contained in the search field. */
  @property({ type: String })
  searchText = '';

  /** Whether line wrapping in table cells should be used. */
  @property({ type: Boolean })
  lineWrap = false;

  @property({ type: Array })
  columnData: TableColumn[] = [];

  /** Indicates whether the table content is overflowing to the right. */
  @state()
  private _isOverflowingToRight = false;

  /** A number representing the scroll percentage in the horizontal direction. */
  @state()
  private _scrollPercentageLeft = 0;

  /** A number representing visibility of vertical scroll indicator. */
  @state()
  private _scrollDownOpacity = 0;

  /** Indicates whether to enable autosizing of incoming log entries. */
  @state()
  private _autosizeLocked = false;

  /**
   * Indicates whether to automatically scroll the table container to the bottom
   * when new log entries are added.
   */
  @state()
  private _autoscrollIsEnabled = true;

  @query('.table-container') private _tableContainer!: HTMLDivElement;
  @query('table') private _table!: HTMLTableElement;
  @query('tbody') private _tableBody!: HTMLTableSectionElement;
  @queryAll('tr') private _tableRows!: HTMLTableRowElement[];
  @query('.jump-to-bottom-btn') private _jumpBottomBtn!: HTMLButtonElement;

  /**
   * Data used for column resizing including the column index, the starting
   * mouse position (X-coordinate), and the initial width of the column.
   */
  private columnResizeData: {
    columnIndex: number;
    startX: number;
    startWidth: number;
  } | null = null;

  /** The number of times the `logs` array has been updated. */
  private logUpdateCount: number = 0;
  /** The maximum number of log entries to render in the list. */
  private readonly MAX_ENTRIES = 100_000;
  /** The maximum number of log updates until autosize is disabled. */
  private readonly AUTOSIZE_LIMIT: number = 8;
  /** The minimum width (in px) for table columns. */
  private readonly MIN_COL_WIDTH: number = 52;

  firstUpdated() {
    setInterval(() => this.updateHorizontalOverflowState(), 1000);

    window.addEventListener('scroll', this.handleTableScroll);
    this._tableBody.addEventListener('rangeChanged', this.onRangeChanged);

    const newRowObserver = new MutationObserver(this.onTableRowAdded);
    newRowObserver.observe(this._table, {
      childList: true,
      subtree: true,
    });
  }

  updated(changedProperties: PropertyValues) {
    super.updated(changedProperties);

    if (
      changedProperties.has('offsetWidth') ||
      changedProperties.has('scrollWidth')
    ) {
      this.updateHorizontalOverflowState();
    }

    if (changedProperties.has('logs')) {
      this.logUpdateCount++;
      this.handleTableScroll();
    }

    if (changedProperties.has('columnData')) {
      this.updateColumnWidths();
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this._tableContainer.removeEventListener('scroll', this.handleTableScroll);
    this._tableBody.removeEventListener('rangeChanged', this.onRangeChanged);
  }

  private onTableRowAdded = (mutations: MutationRecord[]) => {
    mutations.forEach((mutation) => {
      // Check for added nodes
      if (mutation.type === 'childList' && mutation.addedNodes.length > 0) {
        const addedRows = Array.from(mutation.addedNodes).filter(
          (node) => node.nodeName === 'TR',
        ) as HTMLTableRowElement[];

        if (addedRows.length <= 0) {
          return;
        }

        // Force repaint to prevent flickering
        this._table.offsetTop;

        // Update header row alongside newly-added rows
        const rowsToUpdate = [this._tableRows[0], ...addedRows];

        if (!this._autosizeLocked) {
          this.autosizeColumns();
        } else {
          this.updateColumnWidths(rowsToUpdate);
        }

        // Disable auto-sizing once a certain number of updates to the logs array have been made
        if (this.logUpdateCount >= this.AUTOSIZE_LIMIT) {
          this._autosizeLocked = true;
        }
      }
    });
  };

  /** Called when the Lit virtualizer updates its range of entries. */
  private onRangeChanged = () => {
    if (this._autoscrollIsEnabled) {
      this.scrollTableToBottom();
    }
  };

  /** Scrolls to the bottom of the table container. */
  private scrollTableToBottom() {
    const container = this._tableContainer;

    // TODO(b/298097109): Refactor `setTimeout` usage
    setTimeout(() => {
      container.scrollTop = container.scrollHeight;
    }, 0); // Complete any rendering tasks before scrolling

    this._scrollDownOpacity = 0;
    this._jumpBottomBtn.hidden = true;
  }

  /**
   * Calculates the maximum column widths for the table and updates the table
   * rows.
   */
  private autosizeColumns = (rows = this._tableRows) => {
    // Iterate through each row to find the maximum width in each column
    rows.forEach((row) => {
      const cells = Array.from(row.children).filter(
        (cell) => !cell.hasAttribute('hidden'),
      ) as HTMLTableCellElement[];

      cells.forEach((cell, columnIndex) => {
        if (columnIndex === 0) return;

        const textLength = cell.textContent?.trim().length || 0;

        if (!this._autosizeLocked) {
          // Update the preferred width if it's smaller than the new one
          if (this.columnData[columnIndex]) {
            this.columnData[columnIndex].characterLength = Math.max(
              this.columnData[columnIndex].characterLength,
              textLength,
            );
          } else {
            // Initialize if the column data for this index does not exist
            this.columnData[columnIndex] = {
              fieldName: '',
              characterLength: textLength,
              manualWidth: null,
              isVisible: true,
            };
          }
        }
      });
    });

    this.updateColumnWidths(rows);
  };

  private generateGridTemplateColumns(
    newWidth?: number,
    resizingIndex?: number,
  ): string {
    let gridTemplateColumns = '';

    this.columnData.forEach((col, i) => {
      let columnValue = '';

      if (col.isVisible) {
        if (i === resizingIndex) {
          columnValue = `${newWidth}px`;
        } else if (col.manualWidth !== null) {
          columnValue = `${col.manualWidth}px`;
        } else {
          if (i === 0) {
            columnValue = '3.25rem';
          } else {
            const chWidth = col.characterLength;
            const padding = 34;
            columnValue = `clamp(${this.MIN_COL_WIDTH}px, ${chWidth}ch + ${padding}px, 80ch)`;
          }
        }

        gridTemplateColumns += columnValue + ' ';
      }
    });

    return gridTemplateColumns.trim();
  }

  private updateColumnWidths(rows: HTMLTableRowElement[] = this._tableRows) {
    const gridTemplateColumns = this.generateGridTemplateColumns();

    for (const row of rows) {
      row.style.gridTemplateColumns = gridTemplateColumns;
    }
  }

  /**
   * Highlights text content within the table cell based on the current filter
   * value.
   *
   * @param {string} text - The table cell text to be processed.
   */
  private highlightMatchedText(text: string): TemplateResult[] {
    const noQuotesText = this.searchText.replace(/(^"|')|("|'$)/g, '');
    if (!noQuotesText) {
      return [html`${text}`];
    }

    const escapedsearchText = noQuotesText.replace(
      /[.*+?^${}()|[\]\\]/g,
      '\\$&',
    );
    const regex = new RegExp(`(${escapedsearchText})`, 'gi');
    const parts = text.split(regex);
    return parts.map((part) =>
      regex.test(part) ? html`<mark>${part}</mark>` : html`${part}`,
    );
  }

  /** Updates horizontal overflow state. */
  private updateHorizontalOverflowState() {
    const containerWidth = this.offsetWidth;
    const tableWidth = this._tableContainer.scrollWidth;

    this._isOverflowingToRight = tableWidth > containerWidth;
  }

  /**
   * Calculates scroll-related properties and updates the component's state when
   * the user scrolls the table.
   */
  private handleTableScroll = () => {
    const container = this._tableContainer;
    const containerWidth = container.offsetWidth;
    const scrollLeft = container.scrollLeft;
    const scrollY =
      container.scrollHeight - container.scrollTop - container.clientHeight;
    const maxScrollLeft = container.scrollWidth - containerWidth;
    const rowHeight = this._tableRows[0].offsetHeight;

    this._scrollPercentageLeft = scrollLeft / maxScrollLeft || 0;

    if (Math.abs(scrollY) <= 1) {
      this._autoscrollIsEnabled = true;
      return;
    }

    if (Math.round(scrollY - rowHeight) >= 1) {
      this._autoscrollIsEnabled = false;
    }

    let debounceTimer: NodeJS.Timer | null = null;
    if (debounceTimer) {
      clearTimeout(debounceTimer);
    }

    debounceTimer = setTimeout(() => {
      if (Math.round(scrollY - rowHeight) >= 1) {
        this._jumpBottomBtn.hidden = false;
        this._scrollDownOpacity = 1;
      } else {
        this._jumpBottomBtn.hidden = true;
        this._scrollDownOpacity = 0;
      }
    }, 100);
  };

  /**
   * Handles column resizing.
   *
   * @param {MouseEvent} event - The mouse event triggered during column
   *   resizing.
   * @param {number} columnIndex - An index specifying the column being resized.
   */
  private handleColumnResizeStart(event: MouseEvent, columnIndex: number) {
    event.preventDefault();

    // Check if the corresponding index in columnData is not visible. If not,
    // check the columnIndex - 1th element until one isn't hidden.
    while (
      this.columnData[columnIndex] &&
      !this.columnData[columnIndex].isVisible
    ) {
      columnIndex--;
      if (columnIndex < 0) {
        // Exit the loop if we've checked all possible columns
        return;
      }
    }

    // If no visible columns are found, return early
    if (columnIndex < 0) return;

    const startX = event.clientX;
    const columnHeader = this._table.querySelector(
      `th:nth-child(${columnIndex + 1})`,
    ) as HTMLTableCellElement;

    if (!columnHeader) return;

    const startWidth = columnHeader.offsetWidth;

    this.columnResizeData = {
      columnIndex: columnIndex,
      startX,
      startWidth,
    };

    const handleColumnResize = (event: MouseEvent) => {
      this.handleColumnResize(event);
    };

    const handleColumnResizeEnd = () => {
      this.columnResizeData = null;
      document.removeEventListener('mousemove', handleColumnResize);
      document.removeEventListener('mouseup', handleColumnResizeEnd);

      // Communicate column data changes back to parent Log View
      const updateColumnData = new CustomEvent('update-column-data', {
        detail: this.columnData,
      });

      this.dispatchEvent(updateColumnData);
    };

    document.addEventListener('mousemove', handleColumnResize);
    document.addEventListener('mouseup', handleColumnResizeEnd);
  }

  /**
   * Adjusts the column width during a column resize.
   *
   * @param {MouseEvent} event - The mouse event object.
   */
  private handleColumnResize(event: MouseEvent) {
    if (!this.columnResizeData) return;

    const { columnIndex, startX, startWidth } = this.columnResizeData;
    const offsetX = event.clientX - startX;
    const newWidth = Math.max(startWidth + offsetX, this.MIN_COL_WIDTH);

    // Ensure the column index exists in columnData
    if (this.columnData[columnIndex]) {
      this.columnData[columnIndex].manualWidth = newWidth;
    }

    const gridTemplateColumns = this.generateGridTemplateColumns(
      newWidth,
      columnIndex,
    );

    // Update the grid layout for each row
    this._tableRows.forEach((row) => {
      row.style.gridTemplateColumns = gridTemplateColumns;
    });
  }

  render() {
    const logsDisplayed: LogEntry[] = this.logs.slice(0, this.MAX_ENTRIES);

    return html`
      <div
        class="table-container"
        role="log"
        @scroll="${this.handleTableScroll}"
      >
        <table>
          <thead>
            ${this.tableHeaderRow()}
          </thead>

          <tbody>
            ${virtualize({
              items: logsDisplayed,
              renderItem: (log) => html`${this.tableDataRow(log)}`,
            })}
          </tbody>
        </table>
        ${this.overflowIndicators()}
      </div>
      <md-filled-button
        class="jump-to-bottom-btn"
        title="Jump to Bottom"
        @click="${this.scrollTableToBottom}"
        leading-icon
        hidden
      >
        <md-icon slot="icon" aria-hidden="true">arrow_downward</md-icon>
        Jump to Bottom
      </md-filled-button>
    `;
  }

  private tableHeaderRow() {
    return html`
      <tr>
        ${this.columnData.map((columnData, columnIndex) =>
          this.tableHeaderCell(
            columnData.fieldName,
            columnIndex,
            columnData.isVisible,
          ),
        )}
      </tr>
    `;
  }

  private tableHeaderCell(
    fieldKey: string,
    columnIndex: number,
    isVisible: boolean,
  ) {
    return html`
      <th title="${fieldKey}" ?hidden=${!isVisible}>
        ${fieldKey}
        ${columnIndex > 0 ? this.resizeHandle(columnIndex - 1) : html``}
      </th>
    `;
  }

  private resizeHandle(columnIndex: number) {
    if (columnIndex === 0) {
      return html`
        <span class="resize-handle" style="pointer-events: none"></span>
      `;
    }

    return html`
      <span
        class="resize-handle"
        @mousedown="${(event: MouseEvent) =>
          this.handleColumnResizeStart(event, columnIndex)}"
      ></span>
    `;
  }

  private tableDataRow(log: LogEntry) {
    const classes = {
      'log-row': true,
      'log-row--nowrap': !this.lineWrap,
    };
    const logSeverityClass = ('log-row--' +
      (log.severity || Severity.INFO).toLowerCase()) as keyof typeof classes;
    classes[logSeverityClass] = true;

    return html`
      <tr class="${classMap(classes)}">
        ${this.columnData.map((columnData, columnIndex) =>
          this.tableDataCell(
            log,
            columnData.fieldName,
            columnIndex,
            columnData.isVisible,
          ),
        )}
      </tr>
    `;
  }

  private tableDataCell(
    log: LogEntry,
    fieldKey: string,
    columnIndex: number,
    isVisible: boolean,
  ) {
    const field = log.fields.find((f) => f.key === fieldKey) || {
      key: fieldKey,
      value: '',
    };

    if (field.key == 'severity') {
      const severityIcons = new Map<Severity, string>([
        [Severity.WARNING, 'warning'],
        [Severity.ERROR, 'cancel'],
        [Severity.CRITICAL, 'brightness_alert'],
        [Severity.DEBUG, 'bug_report'],
      ]);

      const severityValue = field.value as Severity;
      const iconId = severityIcons.get(severityValue) || '';
      const toTitleCase = (input: string): string => {
        return input.replace(/\b\w+/g, (match) => {
          return match.charAt(0).toUpperCase() + match.slice(1).toLowerCase();
        });
      };

      return html`
        <td ?hidden=${!isVisible}>
          <div class="cell-content cell-content--icon">
            <md-icon
              class="cell-icon"
              title="${toTitleCase(field.value.toString())}"
            >
              ${iconId}
            </md-icon>
          </div>
        </td>
      `;
    }

    return html`
      <td ?hidden=${!isVisible}>
        <div class="cell-content">
          <span class="cell-text"
            >${this.highlightMatchedText(field.value.toString())}</span
          >
        </div>
        ${columnIndex > 0 ? this.resizeHandle(columnIndex - 1) : html``}
      </td>
    `;
  }

  private overflowIndicators = () => html`
    <div
      class="bottom-indicator"
      style="opacity: ${this._scrollDownOpacity}"
    ></div>

    <div
      class="overflow-indicator left-indicator"
      style="opacity: ${this._scrollPercentageLeft}"
      ?hidden="${!this._isOverflowingToRight}"
    ></div>

    <div
      class="overflow-indicator right-indicator"
      style="opacity: ${1 - this._scrollPercentageLeft}"
      ?hidden="${!this._isOverflowingToRight}"
    ></div>
  `;
}
