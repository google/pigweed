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

/* eslint-disable prefer-const */

import * as assert from 'assert';

import { OK, RefreshManager } from './refreshManager';

suite('callback registration', () => {
  test('callback registered for state is called on transition', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called = false;

    manager.on(() => {
      called = true;
      return OK;
    }, 'willRefresh');

    await manager.move('willRefresh');
    assert.ok(called);
  });

  test('callback is called every time', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called = 0;

    manager.on(() => {
      called++;
      return OK;
    }, 'abort');

    await manager.move('abort');
    assert.equal(1, called);

    await manager.move('abort');
    assert.equal(2, called);
  });

  test('transient callback is run only once', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called = 0;

    manager.onOnce(() => {
      called++;
      return OK;
    }, 'abort');

    await manager.move('abort');
    assert.equal(1, called);

    await manager.move('abort');
    assert.equal(1, called);
  });

  test('callback registered for state is not called on other state transition', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called = false;

    manager.on(() => {
      called = true;
      return OK;
    }, 'refreshing');

    await manager.move('willRefresh');
    assert.ok(!called);
  });

  test('callback registered for state transition is called on transition', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called = false;

    manager.on(
      () => {
        called = true;
        return OK;
      },
      'refreshing',
      'willRefresh',
    );

    const managerWillRefresh = await manager.move('willRefresh');
    assert.ok(!called);

    await managerWillRefresh.move('refreshing');
    assert.ok(called);
  });

  test('callback registered for state transition is not called on different transition', async () => {
    const manager1 = RefreshManager.create('refreshing', {
      useRefreshSignalHandler: false,
    });
    const manager2 = RefreshManager.create('didRefresh', {
      useRefreshSignalHandler: false,
    });
    let called = false;

    const cb = () => {
      called = true;
      return OK;
    };

    manager1.on(cb, 'abort', 'didRefresh');
    manager2.on(cb, 'abort', 'didRefresh');

    await manager1.move('abort');
    assert.ok(!called);

    await manager2.move('abort');
    assert.ok(called);
  });

  test('multiple callbacks are called successfully', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called1 = false;
    let called2 = false;

    manager.on(() => {
      called1 = true;
      return OK;
    }, 'willRefresh');

    manager.on(() => {
      called2 = true;
      return OK;
    }, 'willRefresh');

    await manager.move('willRefresh');
    assert.ok(called1);
    assert.ok(called2);
  });

  test('callback error terminates execution', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called1 = false;
    let called2 = false;

    manager.on(() => {
      called1 = true;
      return OK;
    }, 'willRefresh');

    manager.on(() => {
      return { error: 'oh no' };
    }, 'willRefresh');

    await manager.move('willRefresh');
    assert.equal(true, called1);
    assert.equal(false, called2);
  });

  test('callback error precludes calling remaining callbacks', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let called1 = false;
    let called2 = false;

    manager.on(() => {
      return { error: 'oh no' };
    }, 'willRefresh');

    manager.on(() => {
      called1 = true;
      return OK;
    }, 'willRefresh');

    await manager.move('willRefresh');
    assert.ok(!called1);
    assert.ok(!called2);
  });
});

suite('state transitions', () => {
  test('moves through states successfully', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let willRefreshHappened = false;
    let refreshingHappened = false;
    let didRefreshHappened = false;
    let idleHappened = false;

    manager.on(
      () => {
        willRefreshHappened = true;
        return OK;
      },
      'willRefresh',
      'idle',
    );

    manager.on(
      () => {
        refreshingHappened = true;
        return OK;
      },
      'refreshing',
      'willRefresh',
    );

    manager.on(
      () => {
        didRefreshHappened = true;
        return OK;
      },
      'didRefresh',
      'refreshing',
    );

    manager.on(
      () => {
        idleHappened = true;
        return OK;
      },
      'idle',
      'didRefresh',
    );

    await manager.start();

    assert.ok(willRefreshHappened);
    assert.ok(refreshingHappened);
    assert.ok(didRefreshHappened);
    assert.ok(idleHappened);
    assert.equal('idle', manager.state);
  });

  test('fault state prevents downstream execution', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let willRefreshHappened = false;
    let refreshingHappened = false;
    let didRefreshHappened = false;
    let idleHappened = false;

    manager.on(
      () => {
        willRefreshHappened = true;
        return OK;
      },
      'willRefresh',
      'idle',
    );

    manager.on(
      () => {
        refreshingHappened = true;
        return OK;
      },
      'refreshing',
      'willRefresh',
    );

    manager.on(
      () => {
        return { error: 'oh no' };
      },
      'didRefresh',
      'refreshing',
    );

    manager.on(
      () => {
        idleHappened = true;
        return OK;
      },
      'idle',
      'didRefresh',
    );

    await manager.start();

    assert.ok(willRefreshHappened);
    assert.ok(refreshingHappened);
    assert.ok(!didRefreshHappened);
    assert.ok(!idleHappened);
    assert.equal('fault', manager.state);
  });

  test('can start from fault state', async () => {
    const manager = RefreshManager.create('fault', {
      useRefreshSignalHandler: false,
    });
    let willRefreshHappened = false;
    let refreshingHappened = false;
    let didRefreshHappened = false;
    let idleHappened = false;

    manager.on(
      () => {
        willRefreshHappened = true;
        return OK;
      },
      'willRefresh',
      'idle',
    );

    manager.on(
      () => {
        refreshingHappened = true;
        return OK;
      },
      'refreshing',
      'willRefresh',
    );

    manager.on(
      () => {
        didRefreshHappened = true;
        return OK;
      },
      'didRefresh',
      'refreshing',
    );

    manager.on(
      () => {
        idleHappened = true;
        return OK;
      },
      'idle',
      'didRefresh',
    );

    await manager.start();

    assert.ok(willRefreshHappened);
    assert.ok(refreshingHappened);
    assert.ok(didRefreshHappened);
    assert.ok(idleHappened);
    assert.equal('idle', manager.state);
  });

  test('abort signal prevents downstream execution', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let willRefreshHappened = false;
    let idleHappened = false;

    manager.on(
      async () => {
        await new Promise((resolve) => setTimeout(resolve, 25));
        willRefreshHappened = true;
        return OK;
      },
      'willRefresh',
      'idle',
    );

    manager.on(
      async () => {
        await new Promise((resolve) => setTimeout(resolve, 25));
        return OK;
      },
      'refreshing',
      'willRefresh',
    );

    manager.on(
      async () => {
        await new Promise((resolve) => setTimeout(resolve, 25));
        return OK;
      },
      'didRefresh',
      'refreshing',
    );

    manager.on(
      async () => {
        await new Promise((resolve) => setTimeout(resolve, 25));
        idleHappened = true;
        return OK;
      },
      'idle',
      'didRefresh',
    );

    await Promise.all([
      manager.start(),
      new Promise((resolve) =>
        setTimeout(() => {
          manager.abort();
          resolve(null);
        }, 50),
      ),
    ]);

    assert.ok(willRefreshHappened);
    assert.ok(!idleHappened);
    assert.equal('idle', manager.state);
  });

  test('abort signal interrupts callback chain', async () => {
    const manager = RefreshManager.create({ useRefreshSignalHandler: false });
    let calls = 0;

    const cb = async () => {
      await new Promise((resolve) => setTimeout(resolve, 25));
      calls++;
      return OK;
    };

    manager.on(cb, 'willRefresh', 'idle');
    manager.on(cb, 'willRefresh', 'idle');
    manager.on(cb, 'willRefresh', 'idle');
    manager.on(cb, 'willRefresh', 'idle');

    await Promise.all([
      manager.start(),
      new Promise((resolve) =>
        setTimeout(() => {
          manager.abort();
          resolve(null);
        }, 50),
      ),
    ]);

    assert.ok(calls < 4);
  });

  test('starting refresh waits on idle state', async () => {
    // This is a tricky test.
    // We want to see that a particular callback is called twice, because we
    // refresh twice: once with a manual trigger, then again by setting the
    // refresh signal. If the callback is called twice and we didn't get any
    // errors, then we can assume that the signal-triggered refresh waited for
    // the manually-triggered refresh to finish and enter the idle state before
    // starting.
    let calls = 0;

    // We need to keep this test alive until the signal-triggered refresh is
    // done, but we don't want to block in the callback that sends the signal by
    // awaiting the refresh in that callback (if we do, the test will time out
    // because the manually-triggered refresh will never complete). So we
    // trigger handling the signal in the callback without awaiting it, store
    // the promise here, and await it at the end to make sure both refreshes
    // are complete before the test ends.
    let handleRefreshPromise: Promise<void>;

    const manager = RefreshManager.create({ useRefreshSignalHandler: false });

    // Increment the number of calls when this callback is called. We expect
    // this to happen once during the manually-triggered refresh, then again
    // during the signal-driven refresh.
    manager.on(async () => {
      calls++;
      return OK;
    }, 'willRefresh');

    // This is kind of ugly and shouldn't be a model for application code, but
    // it gets the job done in this test. After incrementing the call count
    // in the stage above, in the next stage we activate the refresh signal
    // and directly invoke the signal handler (instead of relying on the
    // periodic refresh signal handler). As described above, we don't want to
    // await the handler here because that essentially creates a deadlock. So
    // we store the promise outside of the callback.
    manager.on(async () => {
      // Limit the number of times this is called to prevent infinite refresh.
      if (calls < 2) {
        manager.refresh();
        handleRefreshPromise = manager.handleRefreshSignal();
      }
      return OK;
    }, 'refreshing');

    // This starts the manually-triggered refresh.
    await manager.start();

    // This awaits the handler for the signal-triggered refresh.
    await handleRefreshPromise!;

    assert.equal(2, calls);
  });

  test('refresh signal triggers from fault state', async () => {
    const manager = RefreshManager.create('fault', {
      useRefreshSignalHandler: false,
    });

    let called = false;

    manager.on(async () => {
      called = true;
      return OK;
    }, 'willRefresh');

    manager.refresh();
    await manager.handleRefreshSignal();

    assert.ok(called);
  });
});
