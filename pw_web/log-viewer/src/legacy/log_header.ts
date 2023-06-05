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

import * as constant from "./models/constants";
import { html, LitElement } from "lit";
import { customElement } from "lit/decorators.js";
import {
    LogField,
    LogHeadersData,
    HeaderWidthChangeHandler,
} from "./models/fields";

@customElement("log-header")
export class LogHeader extends LitElement {
    private maxWidths = {} as Record<string, number>;
    private headerKeys: LogField[];

    constructor(
        private headerFields: LogHeadersData,
        private onHeaderWidthChange: HeaderWidthChangeHandler
    ) {
        super();
        const fields = this.headerFields;
        const keys = Object.keys(fields).map((field) => field as LogField);
        this.headerKeys = keys;
        for (let i = 0; i < keys.length; i++) {
            const header = keys[i] as LogField;
            this.maxWidths[header] = 0;
        }
    }

    render() {
        const fields = this.headerFields;
        return html`
            ${this.headerKeys.map((field, i) => {
                return html`
                    <div id="${field}" style="width: ${fields[field].width}">
                        ${fields[field].displayName}
                        ${i !== this.headerKeys.length - 1
                            ? html` <div class="column-resize"></div> `
                            : ""}
                    </div>
                `;
            })}
        `;
    }

    protected createRenderRoot() {
        return this;
    }

    public setMaxCellWidth(id: LogField, maxWidth: number): void {
        this.maxWidths[id] = maxWidth;
    }

    firstUpdated(changedProperties: Map<PropertyKey, unknown>) {
        super.firstUpdated(changedProperties);
        const resizers = this.querySelectorAll(".column-resize");
        for (const index in Array.from(resizers)) {
            const curResizer = resizers[index];
            const headerEl = curResizer.parentElement!;
            const field = headerEl!.id as LogField;
            const fieldWidth = this.headerFields[field].width;

            // Check for percentage and convert to pixels.
            if (fieldWidth.charAt(fieldWidth.length - 1) === "%") {
                const pixels = Math.round(
                    (headerEl.parentElement!.clientWidth *
                        parseInt(fieldWidth)) /
                        100
                );
                headerEl.style.width = `${pixels}px`;
            } else {
                headerEl.style.width = `${this.headerFields[field].width}px`;
            }

            createResizableColumn(
                this.onHeaderWidthChange,
                this.maxWidths,
                curResizer.parentElement!,
                curResizer as HTMLElement
            );
        }
    }
}

/**
 * Creates resizable element in header to determine column size.
 * @param header identify which header is active.
 * @param cell header cell of the column.
 * @param resizer element in the header cell that handles the resizing.
 */
function createResizableColumn(
    onHeaderWidthChange: HeaderWidthChangeHandler,
    maxWidths: Record<string, number>,
    cell: HTMLElement,
    resizer: HTMLElement
) {
    const header = cell.id as LogField;
    let w = 0;
    let x = 0;

    const mouseDoubleClickHandler = function () {
        cell.style.width = `${maxWidths[header]}px`;
        onHeaderWidthChange(header, cell.style.width);
    };

    const mouseDownHandler = function (e: MouseEvent) {
        x = e.clientX;
        w = cell.clientWidth;

        document.addEventListener("mousemove", mouseMoveHandler);
        document.addEventListener("mouseup", mouseUpHandler);

        resizer.classList.add("resizing");
    };

    const mouseMoveHandler = function (e: MouseEvent) {
        const dx = e.clientX - x;
        if (w + dx > constant.COL_MINWIDTH) {
            cell.style.width = `${w + dx}px`;
            onHeaderWidthChange(header, cell.style.width);
        }
    };

    const mouseUpHandler = function () {
        resizer.classList.remove("resizing");
        document.removeEventListener("mousemove", mouseMoveHandler);
        document.removeEventListener("mouseup", mouseUpHandler);
    };

    resizer.addEventListener("mousedown", mouseDownHandler);
    resizer.addEventListener("dblclick", mouseDoubleClickHandler);
}
