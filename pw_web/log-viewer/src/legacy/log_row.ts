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

import { ChildPart, html, LitElement } from "lit";
import { Directive, directive } from "lit/directive.js";
import { customElement, property, state } from "lit/decorators.js";
import { LogColumnFormatter, LogHeadersData } from "./models/fields";
import { LogRowData } from "./models/log_data";

class AttributeSetter extends Directive {
    update(part: ChildPart, [attributes]: [any]) {
        const domElement = part.parentNode as Element;
        for (const attr in attributes) {
            const value = attributes[attr];
            domElement.setAttribute(`data-${attr}`, value);
        }
        return this.render(attributes);
    }

    render(_attributes: any) {
        return "";
    }
}
@customElement("log-view-row")
export class LogRow extends LitElement {
    @property({ attribute: true, type: Object })
    public log: LogRowData | undefined = undefined;

    @state()
    private fields!: Record<string, any>;
    private dataAttributes!: Record<string, any>;

    constructor(
        log: LogRowData,
        private headerFields: LogHeadersData,
        private columnFormatter: LogColumnFormatter
    ) {
        super();
        this.log = log;
        this.formatRow(this.log);
    }

    createRenderRoot() {
        return this;
    }

    render() {
        const attributeSetter = directive(AttributeSetter);
        return html` <div class="log-entry">
            ${attributeSetter(this.dataAttributes)}
            ${Object.keys(this.headerFields).map(
                this.htmlForFieldKey.bind(this)
            )}
        </div>`;
    }

    private htmlForFieldKey(fieldKey: string) {
        const content = this.fields[fieldKey] ?? "";
        return html` <div
            id="${fieldKey}"
            title="${fieldKey === "message" ? "" : content}"
            class="log-list-cell${fieldKey === "message" ? " msg-cell" : ""}"
        >
            ${this.columnFormatter(fieldKey, content)}
        </div>`;
    }

    /**
     * Formats row for Target Log and Symbolized Logs.
     */
    private formatRow(log: LogRowData) {
        const fields = {} as Record<string, any>;
        this.dataAttributes = createAttributes(log);

        let header: string;
        for (header in this.headerFields) {
            const field = header.toLowerCase();
            fields[field] =
                log.fields.find((field) => field.key === header)?.text || "";
        }
        this.fields = fields;
    }
}

/**
 * Creates a map with the attributes to be placed in `data-*` for each log element.
 * @param data the log data associated with the log element
 * @returns a map of attribute key name to its value.
 */
function createAttributes(data: LogRowData): Record<string, any> {
    const attrs: Record<string, any> = {};
    data.fields.forEach((field) => {
        field.dataFields?.forEach((dataField) => {
            attrs[dataField.key] = dataField.value;
        });
    });
    return attrs;
}
