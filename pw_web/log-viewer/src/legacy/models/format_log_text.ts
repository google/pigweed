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

import { FfxEventData, LogPayload } from "./log_data";
import * as constant from "./constants";

/**
 * Returns the log for a log that ffx returned as "malformed".
 * @param text the malformed log data.
 * @returns the text that will be displayed.
 */
export function formatMalformedLog(text: string): string {
    return `Malformed target log: ${text}`;
}

/**
 * Returns the message to display to the user for a ffx event log.
 *
 * @param event the ffx event data.
 * @param hasPreviousLog whether there was a previous log in the stream when this message appeared.
 *        This is used to add additional explanation about potential missed logs.
 * @returns the text to use as the message for this log.
 */
export function messageForEvent(event: FfxEventData, hasPreviousLog: boolean) {
    let result;
    switch (event) {
        case "TargetDisconnected":
            return "Logger lost connection to target. Retrying...";
        case "LoggingStarted":
            result = "Logger started.";
            if (hasPreviousLog) {
                result +=
                    " Logs before this may have been dropped if they were not cached on the " +
                    "target. There may be a brief delay while we catch up...";
            }
            return result;
    }
}

/**
 * Formats a target timestamp as a time that is readable.
 * @param ts the timestamp
 * @returns the formatted time.
 */
export function formatTargetMonotonic(ts: number): string {
    const seconds = ts / constant.NANOS_IN_SECOND;
    return seconds.toFixed(2).padStart(9, "0");
}

/**
 * Formats the payload of a log to present as the message.
 *
 * @param logPayload the payload in the log data that will be formatted.
 * @returns the formatted payload.
 */
export function formatLogPayload(logPayload: LogPayload) {
    let result = "";
    const root = logPayload.root;
    if (root.message) {
        result += root.message.value;
    }
    if (root.keys) {
        result += Object.entries(root.keys)
            .map(([k, v]) => `${k}=${v}`)
            .join(" ");
    }
    if (root.printf) {
        const args = root.printf.args.join(", ");
        result += `${root.printf.format} args=[${args}]`;
    }
    return result;
}

/**
 * Formats the timestamp of a ffx event to display to the user.
 *
 * @param timestamp the local ffx (host) timestamp.
 * @returns the timestamp formatted in a human readable way.
 */
export function formatFfxLocalTimestamp(timestamp: number) {
    const date = new Date(timestamp / 1000000);
    return date.toISOString().replace("T", " ").replace("Z", "");
}
