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

interface IDisposable {
  dispose: () => void;
}

export class Disposable implements IDisposable {
  disposables: IDisposable[] = [];

  dispose = () => {
    this.disposables.forEach((it) => it.dispose());
  };
}

export class Disposer extends Disposable {
  add = <T extends IDisposable>(disposable: T): T => {
    this.disposables.push(disposable);
    return disposable;
  };

  addMany = <T extends Record<string, IDisposable>>(disposables: T): T => {
    this.disposables.push(...Object.values(disposables));
    return disposables;
  };
}
