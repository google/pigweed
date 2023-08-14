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

function hasOwnProperty(obj: object, prop: number | string) {
  if (obj == null) {
    return false;
  }
  //to handle objects with null prototypes (too edge case?)
  return Object.prototype.hasOwnProperty.call(obj, prop);
}
function hasShallowProperty(obj: object, prop: number | string) {
  return (
    (typeof prop === 'number' && Array.isArray(obj)) ||
    hasOwnProperty(obj, prop)
  );
}

function getShallowProperty(obj: object, prop: number | string) {
  if (hasShallowProperty(obj, prop)) {
    return obj[prop];
  }
}
function getKey(key) {
  const intKey = parseInt(key);
  if (intKey.toString() === key) {
    return intKey;
  }
  return key;
}

export function setPathOnObject(
  obj: object,
  path: number | string | Array<number | string>,
  value: any,
  doNotReplace: boolean = false,
) {
  if (typeof path === 'number') {
    path = [path];
  }
  if (!path || path.length === 0) {
    return obj;
  }
  if (typeof path === 'string') {
    return setPathOnObject(
      obj,
      path.split('.').map(getKey),
      value,
      doNotReplace,
    );
  }
  const currentPath = path[0];
  const currentValue = getShallowProperty(obj, currentPath);
  if (path.length === 1) {
    if (currentValue === void 0 || !doNotReplace) {
      obj[currentPath] = value;
    }
    return currentValue;
  }

  if (currentValue === void 0) {
    //check if we assume an array
    if (typeof path[1] === 'number') {
      obj[currentPath] = [];
    } else {
      obj[currentPath] = {};
    }
  }

  return setPathOnObject(obj[currentPath], path.slice(1), value, doNotReplace);
}
