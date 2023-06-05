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

export const NANOS_IN_SECOND = 1_000_000_000;
export const MAX_LOGS = 10_000;

export const MONIKER_FILTER_LABEL = "Moniker:";
export const FFX_MONIKER = "<ffx>";
export const VSCODE_SYNTHETIC_MONIKER = "<VSCode>";

export const SEARCH_PLACEHOLDER = "column:value";

export const COL_MINWIDTH = 10;

export const LOGS_HEADERS = {
    timestamp: {
        displayName: "Timestamp",
        width: "10%",
    },
    pid: {
        displayName: "PID",
        width: "7%",
    },
    tid: {
        displayName: "TID",
        width: "8%",
    },
    moniker: {
        displayName: "Moniker",
        width: "10%",
    },
    tags: {
        displayName: "Tags",
        width: "10%",
    },
    severity: {
        displayName: "Severity",
        width: "8%",
    },
    message: {
        displayName: "Message",
        width: "",
    },
};
