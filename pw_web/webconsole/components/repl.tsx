// Copyright 2022 The Pigweed Authors
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

import {useEffect, useState} from "react";
import {WebSerial} from "pigweedjs";
import "xterm/css/xterm.css";

type WebSerialTransport = WebSerial.WebSerialTransport
const isSSR = () => typeof window === 'undefined';

interface ReplProps {
  transport: WebSerialTransport | undefined
}

const createTerminal = async (container: HTMLElement) => {
  const {Terminal} = await import('xterm');
  const {FitAddon} = await import('xterm-addon-fit');
  const terminal = new Terminal({cursorBlink: true});
  terminal.open(container);

  const fitAddon = new FitAddon();
  terminal.loadAddon(fitAddon);
  fitAddon.fit();
  return terminal;
};

export default function Repl({transport}: ReplProps) {
  const [terminal, setTerminal] = useState<any>(null);

  useEffect(() => {
    let cleanupFns: {(): void; (): void;}[] = [];
    if (!terminal && !isSSR() && transport) {
      const futureTerm = createTerminal(document.querySelector('.repl-log-container')!);
      futureTerm.then(async (term) => {
        cleanupFns.push(() => {
          term.dispose();
          setTerminal(null);
        });
        setTerminal(term);
        term.write("// TODO: repl");
      });

      return () => {
        cleanupFns.forEach(fn => fn());
      }
    }
    else if (terminal && !transport) {
      terminal.dispose();
      setTerminal(null);
    }
  }, [transport]);

  return (
    <div className="repl-log-container" style={{height: "100%", width: "100%"}}></div>
  )
}
