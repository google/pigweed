// Copyright 2020 The Pigweed Authors
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

import Button from '@material-ui/core/Button';
import * as React from 'react';
import { WebSerialTransport } from '../transport/web_serial_transport';


export function App() {
    const transport = new WebSerialTransport();

    transport.chunks.subscribe((item) => {
        console.log(item);
    })

    return (
        <div className="app">
            <h1>Example Page</h1>
            <Button variant="contained" color="primary" onClick={() => {
                transport.connect();
            }}>Connect</Button>
        </div>);
}
