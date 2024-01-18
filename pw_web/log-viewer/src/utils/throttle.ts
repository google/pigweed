// Copyright 2024 The Pigweed Authors
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

export function throttle<T extends unknown[]>(
  func: (...args: T) => void,
  wait: number,
): (...args: T) => void {
  let timeout: ReturnType<typeof setTimeout> | null = null;
  let lastArgs: T | null = null;
  let lastCallTime: number | null = null;

  const invokeFunction = (args: T) => {
    lastCallTime = Date.now();
    func(...args);
    timeout = null;
  };

  return function (...args: T) {
    const now = Date.now();
    const remainingWait = lastCallTime ? wait - (now - lastCallTime) : 0;

    lastArgs = args;

    if (remainingWait <= 0 || remainingWait > wait) {
      if (timeout) {
        clearTimeout(timeout);
        timeout = null;
      }
      invokeFunction(args);
    } else if (!timeout) {
      timeout = setTimeout(() => {
        invokeFunction(lastArgs as T);
      }, remainingWait);
    }
  };
}
