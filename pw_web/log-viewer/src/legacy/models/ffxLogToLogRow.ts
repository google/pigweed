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
import {
    FfxLogData,
    LogData,
    FfxEventData,
    LogRowData,
    LogRowValues,
    LogRowDataField,
} from "./log_data";
import {
    formatLogPayload,
    formatMalformedLog,
    formatTargetMonotonic,
    messageForEvent,
} from "./format_log_text";
import { LogField } from "./fields";

/**
 * Converts Ffx log entry to LogRowData which the UI component accepts.
 */
export function ffxLogToLogRowData(
    log: FfxLogData,
    hasPreviousLog?: boolean
): LogRowData | undefined {
    if (log.data.TargetLog !== undefined) {
        return formatRow(log.data.TargetLog);
    } else if (log.data.FfxEvent !== undefined) {
        return formatFfxEventRow(log.data.FfxEvent, hasPreviousLog);
    } else if (log.data.MalformedTargetLog !== undefined) {
        return formatMalformedRow(log.data.MalformedTargetLog);
    } else if (log.data.SymbolizedTargetLog !== undefined) {
        return formatRow(
            log.data.SymbolizedTargetLog[0],
            log.data.SymbolizedTargetLog[1]
        );
    } else if (log.data.ViewerEvent !== undefined) {
        return formatViewerEventRow(log.data.ViewerEvent);
    }
    return undefined;
}

/**
 * Formats row for Ffx Logs.
 */
function formatFfxEventRow(
    event: FfxEventData,
    hasPreviousLog = false
): LogRowData {
    const fields: LogRowValues[] = [
        {
            key: "moniker",
            text: constant.FFX_MONIKER,
            dataFields: [{ key: "moniker", value: constant.FFX_MONIKER }],
        },
        { key: "message", text: messageForEvent(event, hasPreviousLog) },
    ];
    return { fields };
}

/**
 * Formats row for Malformed Logs.
 * @param malformedLog the malformed log to render as a message.
 */
function formatMalformedRow(malformedLog: string): LogRowData {
    const fields: LogRowValues[] = [
        { key: "message", text: formatMalformedLog(malformedLog) },
    ];
    return { fields };
}

/**
 *  Formats row for an viewer synthesizedd log.
 * @param message  the message to display.
 */
function formatViewerEventRow(message: string): LogRowData {
    const fields: LogRowValues[] = [
        {
            key: "moniker",
            text: constant.VSCODE_SYNTHETIC_MONIKER,
            dataFields: [
                { key: "moniker", value: constant.VSCODE_SYNTHETIC_MONIKER },
            ],
        },
        { key: "message", text: message },
    ];
    return { fields };
}

/**
 * Formats row for Target Log and Symbolized Logs.
 */
export function formatRow(log: LogData, symbolized?: string): LogRowData {
    const fields: LogRowValues[] = [];

    let header: LogField;
    for (header in constant.LOGS_HEADERS) {
        const field = header.toLowerCase();
        fields.push(getLogCell(log, field, symbolized));
    }
    return { fields };
}

/**
 * Get the field data from a cell in the Log Viewer table.
 *
 * @param logColumn the field data that determines the column of the table.
 * @returns cell element as a string.
 */
function getLogCell(
    log: LogData,
    logColumn: string,
    symbolized: string | undefined
): LogRowValues {
    let time, pid, tid, tags, msg, urlResult, dataFields: LogRowDataField[];
    switch (logColumn) {
        case "timestamp":
            time = formatTargetMonotonic(log.metadata.timestamp);
            return {
                key: logColumn,
                text: time,
                dataFields: [
                    {
                        key: "timestamp",
                        value: log.metadata.timestamp.toString(),
                    },
                ],
            };
        case "pid":
            pid = log.metadata.pid ?? "";
            return {
                key: logColumn,
                text: pid.toString(),
                dataFields: [{ key: logColumn, value: pid }],
            };
        case "tid":
            tid = log.metadata.tid ?? "";
            return {
                key: logColumn,
                text: tid.toString(),
                dataFields: [{ key: logColumn, value: tid }],
            };
        case "moniker":
            return {
                key: logColumn,
                text: log.moniker,
                dataFields: [{ key: "moniker", value: log.moniker }],
            };
        case "tags":
            tags = (log.metadata.tags ?? []).join(",");
            return {
                key: logColumn,
                text: tags,
                dataFields: [
                    { key: "tags", value: `${log.metadata.tags?.length}` },
                    ...(log.metadata.tags || []).map((tag, index) => {
                        return { key: `tag-${index}`, value: tag };
                    }),
                ],
            };
        case "severity":
            return {
                key: logColumn,
                text: log.metadata.severity,
                dataFields: [
                    {
                        key: "severity",
                        value: log.metadata.severity.toLowerCase(),
                    },
                ],
            };
        case "message":
            msg = symbolized ?? formatLogPayload(log["payload"]!);
            urlResult = COMPONENT_URL_REGEX.exec(log.metadata.component_url);

            dataFields = [];
            if (log.payload.root.message) {
                dataFields.push({
                    key: "message",
                    value: log.payload.root.message.value,
                });
            }
            if (log.payload.root.printf) {
                dataFields.push({
                    key: "message",
                    value: `${
                        log.payload.root.printf
                    } [${log.payload.root.printf.args.join(", ")}]`,
                });
            }
            for (const [key, value] of Object.entries(
                log.payload.root.keys ?? {}
            )) {
                dataFields.push({ key: `custom-${key}`, value });
            }
            if (urlResult) {
                if (urlResult.groups?.package) {
                    dataFields.push({
                        key: "package-name",
                        value: urlResult.groups.package,
                    });
                }
                if (urlResult.groups?.manifest) {
                    dataFields.push({
                        key: "manifest",
                        value: urlResult.groups.manifest,
                    });
                }
            }
            return {
                key: logColumn,
                text: msg,
                dataFields,
            };
        default:
            return { key: logColumn, text: "" };
    }
}

const COMPONENT_URL_REGEX =
    /fuchsia-(pkg|boot):\/\/[^/]*\/(?<package>[^\\]*).*#meta\/(?<manifest>.+\.cmx?)/;
