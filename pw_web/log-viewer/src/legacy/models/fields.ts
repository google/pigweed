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

import * as constant from "./constants";

export type LogField =
    | "timestamp"
    | "pid"
    | "tid"
    | "moniker"
    | "tags"
    | "severity"
    | "message";

export type HeaderWidthChangeHandler = (
    header: LogField,
    width: string
) => void;

export type LogFieldData = {
    displayName: string;
    width: string;
};

export type LogColumnFormatter = (
    fieldName: string,
    text: string
) => HTMLElement | string;

export type LogViewOptions = {
    columnFormatter: LogColumnFormatter;
    showControls: boolean;
};

export type LogHeadersData = Record<LogField, LogFieldData>;

/**
 * Creates a deep copy of the LOGS_HEADERS constant
 * @return deep copy of LOGS_HEADERS
 */
export function copyHeaderConst(): LogHeadersData {
    let field: LogField;
    const copy: LogHeadersData = { ...constant.LOGS_HEADERS };

    for (field in constant.LOGS_HEADERS) {
        copy[field] = { ...constant.LOGS_HEADERS[field] };
    }
    return copy;
}
