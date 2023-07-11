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

import { LogEntry } from './shared/interfaces';

export abstract class LogSource {
    private eventListeners: {
        eventType: string;
        listener: (data: LogEntry) => void;
    }[];

    constructor() {
        this.eventListeners = [];
    }

    addEventListener(
        eventType: string,
        listener: (data: LogEntry) => void
    ): void {
        this.eventListeners.push({ eventType, listener });
    }

    removeEventListener(
        eventType: string,
        listener: (data: LogEntry) => void
    ): void {
        this.eventListeners = this.eventListeners.filter(
            (eventListener) =>
                eventListener.eventType !== eventType ||
                eventListener.listener !== listener
        );
    }

    emitEvent(eventType: string, data: LogEntry): void {
        this.eventListeners.forEach((eventListener) => {
            if (eventListener.eventType === eventType) {
                eventListener.listener(data);
            }
        });
    }
}
