// Copyright 2019 The Pigweed Authors
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

package server

import (
	"errors"
	"fmt"
	"log"
	"os"
	"sync"
	"sync/atomic"
	"time"

	pb "pigweed.dev/module/pw_test_server/gen"
)

// UnitTestRunRequest represents a client request to run a single unit test
// executable.
type UnitTestRunRequest struct {
	// Filesystem path to the unit test executable.
	Path string

	// Channel to which the unit test response is sent back.
	ResponseChannel chan<- *UnitTestRunResponse

	// Time when the request was queued. Internal to the worker pool.
	queueStart time.Time
}

// UnitTestRunResponse is the response sent after a unit test run request is
// processed.
type UnitTestRunResponse struct {
	// Length of time that the run request was queued before being handled
	// by a test worker. Set by the worker pool.
	QueueTime time.Duration

	// Length of time the unit test runner command took to run the test.
	// Set by the worker pool.
	RunTime time.Duration

	// Raw output of the unit test.
	Output []byte

	// Result of the unit test run.
	Status pb.TestStatus

	// Error that occurred during the test run, if any. This is not an error
	// with the unit test (e.g. test failure); rather, an internal error
	// occurring in the test worker pool as it attempted to run the test.
	// If this is not nil, none of the other fields in this struct are
	// guaranteed to be valid.
	Err error
}

// UnitTestRunner represents a worker which handles unit test run requests.
type UnitTestRunner interface {
	// WorkerStart is the lifecycle hook called when the worker routine is
	// started. Any resources required by the worker should be initialized
	// here.
	WorkerStart() error

	// HandleRunRequest is the method called when a unit test is scheduled
	// to run on the worker by the worker pool. It processes the request,
	// runs the unit test, and returns an appropriate response.
	HandleRunRequest(*UnitTestRunRequest) *UnitTestRunResponse

	// WorkerExit is the lifecycle hook called before the worker exits.
	// Should be used to clean up any resources used by the worker.
	WorkerExit()
}

// TestWorkerPool represents a collection of unit test workers which run unit
// test binaries. The worker pool can schedule unit test runs, distributing the
// tests among its available workers.
type TestWorkerPool struct {
	activeWorkers uint32
	logger        *log.Logger
	workers       []UnitTestRunner
	waitGroup     sync.WaitGroup
	testChannel   chan *UnitTestRunRequest
	quitChannel   chan bool
}

var (
	errWorkerPoolActive    = errors.New("Worker pool is running")
	errNoRegisteredWorkers = errors.New("No workers registered in pool")
)

// newWorkerPool creates an empty test worker pool.
func newWorkerPool(name string) *TestWorkerPool {
	logPrefix := fmt.Sprintf("[%s] ", name)
	return &TestWorkerPool{
		logger:      log.New(os.Stdout, logPrefix, log.LstdFlags),
		workers:     make([]UnitTestRunner, 0),
		testChannel: make(chan *UnitTestRunRequest, 1024),
		quitChannel: make(chan bool, 64),
	}
}

// RegisterWorker adds a new unit test worker to the pool which uses the given
// command and arguments to run its unit tests. This cannot be done when the
// pool is processing requests; Stop() must be called first.
func (p *TestWorkerPool) RegisterWorker(worker UnitTestRunner) error {
	if p.Active() {
		return errWorkerPoolActive
	}
	p.workers = append(p.workers, worker)
	return nil
}

// Start launches all registered workers in the pool.
func (p *TestWorkerPool) Start() error {
	if p.Active() {
		return errWorkerPoolActive
	}

	p.logger.Printf("Starting %d workers\n", len(p.workers))
	for _, worker := range p.workers {
		p.waitGroup.Add(1)
		atomic.AddUint32(&p.activeWorkers, 1)
		go p.runWorker(worker)
	}

	return nil
}

// Stop terminates all running workers in the pool. The work queue is not
// cleared; queued requests persist and can be processed by calling Start()
// again.
func (p *TestWorkerPool) Stop() {
	if !p.Active() {
		return
	}

	// Send N quit commands to the workers and wait for them to exit.
	for i := uint32(0); i < p.activeWorkers; i++ {
		p.quitChannel <- true
	}
	p.waitGroup.Wait()

	p.logger.Println("All workers in pool stopped")
}

// Active returns true if any worker routines are currently running.
func (p *TestWorkerPool) Active() bool {
	return p.activeWorkers > 0
}

// QueueTest adds a unit test to the worker pool's queue of tests. If no workers
// are registered in the pool, this operation fails and an immediate response is
// sent back to the requester indicating the error.
func (p *TestWorkerPool) QueueTest(req *UnitTestRunRequest) {
	if len(p.workers) == 0 {
		p.logger.Printf("Attempt to queue test %s with no active workers", req.Path)
		req.ResponseChannel <- &UnitTestRunResponse{
			Err: errNoRegisteredWorkers,
		}
		return
	}

	p.logger.Printf("Queueing unit test %s\n", req.Path)

	// Start tracking how long the request is queued.
	req.queueStart = time.Now()
	p.testChannel <- req
}

// runWorker is a function run by the test worker pool in a separate goroutine
// for each of its registered workers. The function is responsible for calling
// the appropriate worker lifecycle hooks and processing requests as they come
// in through the worker pool's queue.
func (p *TestWorkerPool) runWorker(worker UnitTestRunner) {
	defer func() {
		atomic.AddUint32(&p.activeWorkers, ^uint32(0))
		p.waitGroup.Done()
	}()

	if err := worker.WorkerStart(); err != nil {
		return
	}

processLoop:
	for {
		// Force the quit channel to be processed before the request
		// channel by using a select statement with an empty default
		// case to make the read non-blocking. If the quit channel is
		// empty, the code will fall through to the main select below.
		select {
		case q, ok := <-p.quitChannel:
			if q || !ok {
				break processLoop
			}
		default:
		}

		select {
		case q, ok := <-p.quitChannel:
			if q || !ok {
				break processLoop
			}
		case req, ok := <-p.testChannel:
			if !ok {
				continue
			}

			queueTime := time.Since(req.queueStart)

			runStart := time.Now()
			res := worker.HandleRunRequest(req)
			res.RunTime = time.Since(runStart)

			res.QueueTime = queueTime
			req.ResponseChannel <- res
		}
	}

	worker.WorkerExit()
}
