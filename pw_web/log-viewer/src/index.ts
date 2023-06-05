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

import { MockLogSource } from "./custom/mock-log-source";
import { LogViewer } from "./components/log-viewer";

interface LogEntry {
    hostId: string;
    timestamp: string;
    fields: FieldData[];
}

interface FieldData {
    key: string;
    value: any;
}

import "@material/web/button/filled-button.js";
import "@material/web/button/outlined-button.js";
import "@material/web/checkbox/checkbox.js";
import "@material/web/field/outlined-field.js";
import "@material/web/textfield/outlined-text-field.js";
import "@material/web/textfield/filled-text-field.js";
import "@material/web/iconbutton/standard-icon-button.js";
import "@material/web/icon/icon.js";

const logSource = new MockLogSource();
const logViewer = document.querySelector("log-viewer") as LogViewer; // Get a reference to the <log-viewer> element
const logs: LogEntry[] = [];

const TIMEOUT_DURATION = 5000; // ms

// Define an event listener for the 'logEntry' event
const logEntryListener = (logEntry: LogEntry) => {
    logs.unshift(logEntry);
    logViewer.logs = logs;
    logViewer.requestUpdate("logs", []);
};

// Add the event listener to the LogSource instance
logSource.addEventListener("logEntry", logEntryListener);

// Start reading log data
logSource.start();

// Stop reading log data once timeout duration has elapsed
setTimeout(() => {
    logSource.stop();
    logSource.removeEventListener("logEntry", logEntryListener);
}, TIMEOUT_DURATION);
