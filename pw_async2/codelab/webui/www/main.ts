// Copyright 2025 The Pigweed Authors
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

// The address of the websocket server to connect to
const SOURCE_URL = new URL(document.URL);
SOURCE_URL.protocol = 'ws:';
SOURCE_URL.pathname = '/codelab/webui';
const WEBSOCKET_URL = SOURCE_URL.toString();

// Simulated delay between a motor start and an item dispense event.
const VEND_DELAY_MS = 1500;

// The state for each dispenser
class DispenserSlotState {
  stock = 5;
  motorState: 'on' | 'off' = 'off';
}

class VendingMachineState {
  connected: 'connected' | 'disconnected' = 'disconnected';
  message = 'Welcome to the PigweedCo Vending Machine Demo!';
  credit = 0;
  bank = 0;
  dispensedMessage = 'Last dispensed nothing';
  dispenser: DispenserSlotState[] = [
    new DispenserSlotState(),
    new DispenserSlotState(),
    new DispenserSlotState(),
    new DispenserSlotState(),
  ];
}

class WebUiState {
  connectionStatusDisplay: HTMLElement | null;
  messageDisplay: HTMLElement | null;
  creditValue: HTMLElement | null;
  bankValue: HTMLElement | null;
  dispensedDisplay: HTMLElement | null;

  itemNames: string[];
  stockDisplays: (HTMLElement | null)[];
  motorIndicators: (HTMLElement | null)[];

  constructor() {
    this.connectionStatusDisplay = document.getElementById('status-display');
    this.messageDisplay = document.getElementById('message-display');
    this.creditValue = document.getElementById('credit-value');
    this.bankValue = document.getElementById('bank-value');
    this.dispensedDisplay = document.getElementById('dispensed-display');

    this.itemNames = Array.from(
      { length: 4 },
      (_, i) => document.getElementById(`name-${i}`)?.innerText ?? '',
    );
    this.stockDisplays = Array.from({ length: 4 }, (_, i) =>
      document.getElementById(`stock-${i}`),
    );
    this.motorIndicators = Array.from({ length: 4 }, (_, i) =>
      document.getElementById(`motor-${i}`),
    );
  }

  setConnectionStatusConnected(): void {
    if (this.connectionStatusDisplay) {
      this.connectionStatusDisplay.textContent = 'Connected';
      this.connectionStatusDisplay.className = 'status-display connected';
    }
  }

  setConnectionStatusDisconnected(): void {
    if (this.connectionStatusDisplay) {
      this.connectionStatusDisplay.textContent = 'Disconnected';
      this.connectionStatusDisplay.className = 'status-display disconnected';
    }
  }

  updateUI(state: VendingMachineState): void {
    if (state.connected === 'connected') {
      this.setConnectionStatusConnected();
    } else {
      this.setConnectionStatusDisconnected();
    }

    if (this.dispensedDisplay) {
      this.dispensedDisplay.textContent = state.dispensedMessage;
    }

    if (this.messageDisplay) {
      this.messageDisplay.textContent = state.message;
    }
    if (this.creditValue) {
      this.creditValue.textContent = state.credit.toString();
    }
    if (this.bankValue) {
      this.bankValue.textContent = state.bank.toString();
    }

    this.stockDisplays.forEach((display, i) => {
      if (display) {
        display.textContent = `${state.dispenser[i].stock}`;
      }
    });

    this.motorIndicators.forEach((indicator, i) => {
      if (indicator) {
        indicator.innerText = state.dispenser[i].motorState;
        indicator.classList.toggle(
          'on',
          state.dispenser[i].motorState === 'on',
        );
      }
    });
  }

  onInsertCoin(handler: () => void): void {
    document
      .getElementById('insert-coin-button')
      ?.addEventListener('click', handler);
  }

  onReturnCoins(handler: () => void): void {
    document
      .getElementById('return-coins-button')
      ?.addEventListener('click', handler);
  }

  onKeypadSelection(handler: (slot: number) => void): void {
    for (let slot = 0; slot < 4; slot++) {
      document.getElementById(`vend-${slot}`)?.addEventListener('click', () => {
        handler(slot + 1);
      });
    }
  }

  onDebugDumpDispatcherState(handler: () => void): void {
    document
      .getElementById('debug-dispatcher-button')
      ?.addEventListener('click', handler);
  }

  onQuitButton(handler: () => void): void {
    document.getElementById('quit-button')?.addEventListener('click', handler);
  }
}

class SocketConnection {
  ws?: WebSocket;
  connectedHandler?: () => void;
  disconnectedHandler?: () => void;
  motorStateChangeHandler?: (slot: number, motorState: 'on' | 'off') => void;
  returnCoinsHandler?: () => void;
  bankCoinHandler?: () => void;
  resetHandler?: () => void;
  displayMessageHandler?: (text: string) => void;
  unknownRequestHandler?: (message: string) => void;

  constructor() {
    this.connectInternal();
  }

  connectInternal(): void {
    const ws = new WebSocket(WEBSOCKET_URL);

    ws.onopen = () => {
      console.log('WebSocket backend established.');
      this.connectedHandler?.();
    };

    ws.onmessage = (event) => {
      console.log('Message from server:', event.data);
      try {
        this.parseMessage(event.data);
      } catch (error) {
        console.error('Error parsing message from server:', error);
      }
    };

    ws.onclose = () => {
      console.log('WebSocket backend closed. Reconnecting in 3 seconds...');
      this.disconnectedHandler?.();
      setTimeout(() => {
        this.connectInternal();
      }, 3000);
    };

    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
      ws.close();
    };

    this.ws = ws;
  }

  parseMessage(message: string): void {
    if (message.startsWith('+') || message.startsWith('-')) {
      // "+/-N" Dispenser motor N on ("+N") or off ("-N").
      // e.g., "+1" turns on the motor for slot 1.
      const slot = parseInt(message.substring(1), 10);
      this.motorStateChangeHandler?.(
        slot - 1,
        message.startsWith('+') ? 'on' : 'off',
      );
    } else if (message === 'retc') {
      // "retc" -> Return coins
      this.returnCoinsHandler?.();
    } else if (message === 'bank') {
      // "bank" -> Move credit coins to bank
      this.bankCoinHandler?.();
    } else if (message === 'rset') {
      // "rset" -> Reset the machine state
      this.resetHandler?.();
    } else if (message.startsWith('msg:')) {
      // "msg:text" Set display message to "text"
      const text = message.substring(4);
      this.displayMessageHandler?.(text);
    } else {
      this.unknownRequestHandler?.(message);
    }
  }

  sendMessage(message: string): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(message);
    } else {
      console.error(
        'WebSocket is not open. Ready state:',
        this.ws ? this.ws.readyState : 'uninitialized',
      );
    }
  }

  sendInsertCoin(): void {
    this.sendMessage('c');
  }

  sendReturnCoins(): void {
    this.sendMessage('r');
  }

  sendKeypadPress(slot: number): void {
    this.sendMessage(`${slot}`);
  }

  sendItemDropDetected(): void {
    this.sendMessage(`i`);
  }

  sendDebugDumpDispatcherState(): void {
    this.sendMessage(`d`);
  }

  sendQuit(): void {
    this.sendMessage(`q`);
  }

  onConnected(handler: () => void): void {
    this.connectedHandler = handler;
  }

  onDisconnected(handler: () => void): void {
    this.disconnectedHandler = handler;
  }

  onMotorStateChangeRequested(
    handler: (slot: number, motorState: 'on' | 'off') => void,
  ): void {
    this.motorStateChangeHandler = handler;
  }

  onReturnCoinsRequested(handler: () => void) {
    this.returnCoinsHandler = handler;
  }

  onBankCoinRequested(handler: () => void) {
    this.bankCoinHandler = handler;
  }

  onResetRequested(handler: () => void) {
    this.resetHandler = handler;
  }

  onDisplayMessageRequested(handler: (text: string) => void) {
    this.displayMessageHandler = handler;
  }

  onUnknownRequest(handler: (message: string) => void) {
    this.unknownRequestHandler = handler;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  let machine = new VendingMachineState();
  const ui = new WebUiState();
  const backend = new SocketConnection();

  ui.updateUI(machine);

  function updateUI(): void {
    ui.updateUI(machine);
  }

  ui.onInsertCoin(() => {
    machine.credit++;
    updateUI();
    backend.sendInsertCoin();
  });

  ui.onReturnCoins(() => {
    backend.sendReturnCoins();
  });

  ui.onKeypadSelection((slot: number) => {
    backend.sendKeypadPress(slot);
  });

  ui.onDebugDumpDispatcherState(() => {
    backend.sendDebugDumpDispatcherState();
  });

  ui.onQuitButton(() => {
    backend.sendQuit();
  });

  backend.onConnected(() => {
    machine.connected = 'connected';
    updateUI();
  });

  backend.onDisconnected(() => {
    machine = new VendingMachineState();
    machine.connected = 'disconnected';
    updateUI();
  });

  backend.onMotorStateChangeRequested(
    (slot: number, motorState: 'on' | 'off') => {
      if (slot < 0 || slot >= machine.dispenser.length) {
        machine.dispensedMessage = `Bad dispenser slot ${slot}`;
      }

      const dispenser = machine.dispenser[slot];

      dispenser.motorState = motorState;

      if (motorState === 'on') {
        // Motor turned ON
        if (dispenser.stock > 0) {
          dispenser.stock--;
          updateUI();
          setTimeout(() => {
            backend.sendItemDropDetected();
            machine.dispensedMessage = `${ui.itemNames[slot]} dispensed.`;
          }, VEND_DELAY_MS);
        } else {
          machine.dispensedMessage = `${ui.itemNames[slot]} is out of stock.`;
          // No signal is intentionally sent to the backend for this case.
        }
      }
      updateUI();
    },
  );

  backend.onReturnCoinsRequested(() => {
    machine.dispensedMessage = `Returned ${machine.credit} credits.`;
    machine.credit = 0;
    updateUI();
  });

  backend.onBankCoinRequested(() => {
    const adjust = Math.min(1, machine.credit);
    machine.bank += adjust;
    machine.credit -= adjust;
    updateUI();
  });

  backend.onResetRequested(() => {
    machine = new VendingMachineState();
    machine.connected = 'connected';
    updateUI();
  });

  backend.onDisplayMessageRequested((text: string) => {
    machine.message = text;
    updateUI();
  });

  backend.onUnknownRequest((message: string) => {
    machine.dispensedMessage = `Unknown message from backend: ${message}`;
    updateUI();
  });
});
