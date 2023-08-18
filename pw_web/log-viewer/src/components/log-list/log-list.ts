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
import { FieldData, LogEntry, Severity } from '../../shared/interfaces';
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

  /** The field keys (column values) for the incoming log entries. */
  @state()
  private _fieldKeys = new Set<string>();

  /** Indicates whether the table content is overflowing to the right. */
  @state()
  private _isOverflowingToRight = false;

  /** A number representing the scroll percentage in the horizontal direction. */
  @state()
  private _scrollPercentageLeft = 0;

  /** A number representing visibility of vertical scroll indicator. */
  @state()
  private _scrollDownOpacity = 0;

  /**
   * Indicates whether to automatically scroll the table container to the
   * bottom when new log entries are added.
   */
  @state()
  private _autoscrollIsEnabled = true;

  @query('.jump-to-bottom-btn') private _jumpBottomBtn!: HTMLButtonElement;
  @query('.table-container') private _tableContainer!: HTMLDivElement;
  @query('table') private _table!: HTMLTableElement;
  @query('tbody') private _tableBody!: HTMLTableSectionElement;
  @queryAll('tr') private _tableRows!: HTMLTableRowElement[];

  /**
   * Data used for column resizing including the column index, the starting
   * mouse position (X-coordinate), and the initial width of the column.
   */
  private columnResizeData: {
    columnIndex: number;
    startX: number;
    startWidth: number;
  } | null = null;

  /** The maximum number of log entries to render in the list. */
  private readonly MAX_ENTRIES = 100_000;

  @property({ type: Array })
  colsHidden: (boolean | undefined)[] = [];

  firstUpdated() {
    setInterval(() => this.updateHorizontalOverflowState(), 1000);

    window.addEventListener('scroll', this.handleTableScroll);
    this._tableBody.addEventListener('rangeChanged', this.onRangeChanged);

    if (this.logs.length > 0) {
      this.performUpdate();
    }
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
      this.setFieldNames(this.logs);
      this.handleTableScroll();
    }

    if (changedProperties.has('colsHidden')) {
      this.clearGridTemplateColumns();
      this.updateGridTemplateColumns();
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this._tableContainer.removeEventListener('scroll', this.handleTableScroll);
    this._tableBody.removeEventListener('rangeChanged', this.onRangeChanged);
  }

  /**
   * Sets the field names based on the provided log entries; used to define
   * the table columns.
   *
   * @param logs An array of LogEntry objects.
   */
  private setFieldNames(logs: LogEntry[]) {
    logs.forEach((logEntry) => {
      logEntry.fields.forEach((fieldData) => {
        if (fieldData.key === 'severity') {
          this._fieldKeys.add('');
          return;
        }
        this._fieldKeys.add(fieldData.key);
      });
    });
  }

  /** Called when the Lit virtualizer updates its range of entries. */
  private onRangeChanged = () => {
    this.updateGridTemplateColumns();
    if (this._autoscrollIsEnabled) {
      this.scrollTableToBottom();
    }
  };

  /** Scrolls to the bottom of the table container. */
  private scrollTableToBottom() {
    const container = this._tableContainer;

    // TODO(b/289101398): Refactor `setTimeout` usage
    setTimeout(() => {
      container.scrollTop = container.scrollHeight;
      this._autoscrollIsEnabled = true;
      this._jumpBottomBtn.hidden = true;
      this._scrollDownOpacity = 0;
    }, 0); // Complete any rendering tasks before scrolling
  }

  /** Clears the `gridTemplateColumns` value for all rows in the table. */
  private clearGridTemplateColumns() {
    this._tableRows.forEach((row) => {
      row.style.gridTemplateColumns = '';
    });
  }

  /**
   * Updates column visibility and calculates maximum column widths for the
   * table.
   */
  private updateGridTemplateColumns = () => {
    const rows = this._tableRows;

    // Set column visibility based on `colsHidden` array
    rows.forEach((row) => {
      const cells = Array.from(row.querySelectorAll('td, th'));
      cells.forEach((cell, index: number) => {
        const colHidden = this.colsHidden[index];

        const cellEl = cell as HTMLElement;
        cellEl.hidden = colHidden as boolean;
      });
    });

    // Get the number of visible columns
    const columnCount =
      Array.from(rows[0]?.children || []).filter(
        (child) => !child.hasAttribute('hidden'),
      ).length || 0;

    // Initialize an array to store the maximum width of each column
    const columnWidths: number[] = new Array(columnCount).fill(0);

    // Iterate through each row to find the maximum width in each column
    rows.forEach((row) => {
      const cells = Array.from(row.children).filter(
        (cell) => !cell.hasAttribute('hidden'),
      ) as HTMLTableCellElement[];

      cells.forEach((cell, columnIndex) => {
        const cellWidth = cell.getBoundingClientRect().width;
        columnWidths[columnIndex] = Math.max(
          columnWidths[columnIndex],
          cellWidth,
        );
      });
    });

    // Generate the gridTemplateColumns value for each row
    rows.forEach((row) => {
      const gridTemplateColumns = columnWidths
        .map((width, index) => {
          if (index === columnWidths.length - 1) {
            return '1fr';
          }
          if (index === 0) {
            return '3.25rem';
          }
          return `${width}px`;
        })
        .join(' ');
      row.style.gridTemplateColumns = gridTemplateColumns;
    });
  };

  /**
   * Highlights text content within the table cell based on the current filter
   * value.
   *
   * @param {string} text - The table cell text to be processed.
   */
  private highlightMatchedText(text: string): TemplateResult[] {
    if (!this.searchText) {
      return [html`${text}`];
    }

    const escapedsearchText = this.searchText.replace(
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
   * Calculates scroll-related properties and updates the component's state
   * when the user scrolls the table.
   */
  private handleTableScroll = () => {
    const container = this._tableContainer;
    const containerWidth = container.offsetWidth;
    const scrollLeft = container.scrollLeft;
    const maxScrollLeft = container.scrollWidth - containerWidth;

    this._scrollPercentageLeft = scrollLeft / maxScrollLeft || 0;

    if (
      Math.floor(container.scrollHeight - container.scrollTop) <=
      Math.floor(container.offsetHeight)
    ) {
      this.scrollTableToBottom();
      return;
    }
    this._autoscrollIsEnabled = false;
    this._jumpBottomBtn.hidden = false;
    this._scrollDownOpacity = 1;
    this.requestUpdate();
  };

  /**
   * Handles column resizing.
   *
   * @param {MouseEvent} event - The mouse event triggered during column
   *   resizing.
   * @param {number} columnIndex - An index specifying the column being
   *   resized.
   */
  private handleColumnResizeStart(event: MouseEvent, columnIndex: number) {
    event.preventDefault();

    const startX = event.clientX;
    const columnHeader = this._table.querySelector(
      `th:nth-child(${columnIndex + 1})`,
    ) as HTMLTableCellElement;
    const startWidth = columnHeader.offsetWidth;

    this.columnResizeData = {
      columnIndex,
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
    const columnHeader = this._table.querySelector(
      `th:nth-child(${columnIndex + 1})`,
    ) as HTMLTableCellElement;
    const offsetX = event.clientX - startX;
    const newWidth = Math.max(startWidth + offsetX, 48); // Minimum width
    const totalColumns = this._table.querySelectorAll('th').length;
    let gridTemplateColumns = '';

    columnHeader.style.width = `${newWidth}px`;

    for (let i = 0; i < totalColumns; i++) {
      if (i === columnIndex) {
        gridTemplateColumns += `${newWidth}px `;
        continue;
      }
      const otherColumnHeader = this._table.querySelector(
        `th:nth-child(${i + 1})`,
      ) as HTMLElement;
      const otherColumnWidth = otherColumnHeader.offsetWidth;
      gridTemplateColumns += `${otherColumnWidth}px `;
    }

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
        trailing-icon
      >
        <md-icon slot="icon" aria-hidden="true">arrow_downward</md-icon>
        Jump to Bottom
      </md-filled-button>
    `;
  }

  private tableHeaderRow() {
    return html`
      <tr>
        ${Array.from(this._fieldKeys).map((fieldKey, columnIndex) =>
          this.tableHeaderCell(fieldKey, columnIndex),
        )}
      </tr>
    `;
  }

  private tableHeaderCell(fieldKey: string, columnIndex: number) {
    return html`
      <th>
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
        ${log.fields.map((field, columnIndex) =>
          this.tableDataCell(field, columnIndex),
        )}
      </tr>
    `;
  }

  private tableDataCell(field: FieldData, columnIndex: number) {
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
        <td>
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
      <td>
        <div class="cell-content">
          ${this.highlightMatchedText(field.value.toString())}
        </div>
        <!-- Don't add resize handles for default columns 'severity' and 'timestamp' -->
        ${!['severity', 'timestamp'].includes(field.key) && columnIndex > 0
          ? this.resizeHandle(columnIndex - 1)
          : html``}
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
