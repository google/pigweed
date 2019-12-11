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

// Package server implements a unit test gRPC server which queues and
// distributes unit tests among a group of worker routines.
package server

import (
	"context"
	"errors"
	"fmt"
	"log"
	"net"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/reflection"
	"google.golang.org/grpc/status"

	pb "pigweed.dev/module/pw_test_server/gen"
)

var (
	errServerNotBound   = errors.New("Server not bound to a port")
	errServerNotRunning = errors.New("Server is not running")
)

// Server is a gRPC server that runs a TestServer service.
type Server struct {
	grpcServer  *grpc.Server
	listener    net.Listener
	testsPassed uint32
	testsFailed uint32
	startTime   time.Time
	active      bool
	workerPool  *TestWorkerPool
}

// New creates a gRPC server with a registered TestServer service.
func New() *Server {
	s := &Server{
		grpcServer: grpc.NewServer(),
		workerPool: newWorkerPool("ServerWorkerPool"),
	}

	reflection.Register(s.grpcServer)
	pb.RegisterTestServerServer(s.grpcServer, &pwTestServer{s})

	return s
}

// Bind starts a TCP listener on a specified port.
func (s *Server) Bind(port int) error {
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		return err
	}
	s.listener = lis
	return nil
}

// RegisterWorker adds a unit test worker to the server's worker pool.
func (s *Server) RegisterWorker(worker UnitTestRunner) {
	s.workerPool.RegisterWorker(worker)
}

// RunTest runs a unit test executable through a worker in the test server,
// returning the worker's response. The function blocks until the test has
// been processed.
func (s *Server) RunTest(path string) (*UnitTestRunResponse, error) {
	if !s.active {
		return nil, errServerNotRunning
	}

	resChan := make(chan *UnitTestRunResponse, 1)
	defer close(resChan)

	s.workerPool.QueueTest(&UnitTestRunRequest{
		Path:            path,
		ResponseChannel: resChan,
	})

	res := <-resChan

	if res.Err != nil {
		return nil, res.Err
	}

	if res.Status == pb.TestStatus_SUCCESS {
		s.testsPassed++
	} else {
		s.testsFailed++
	}

	return res, nil
}

// Serve starts the gRPC test server on its configured port. Bind must have been
// called before this; an error is returned if it is not. This function blocks
// until the server is terminated.
func (s *Server) Serve() error {
	if s.listener == nil {
		return errServerNotBound
	}

	log.Printf("Starting gRPC server on %v\n", s.listener.Addr())

	s.startTime = time.Now()
	s.active = true
	s.workerPool.Start()

	return s.grpcServer.Serve(s.listener)
}

// pwTestServer implements the pw.test_server.TestServer gRPC service.
type pwTestServer struct {
	server *Server
}

// RunUnitTest runs a single unit test binary and returns its result.
func (s *pwTestServer) RunUnitTest(
	ctx context.Context,
	desc *pb.UnitTestDescriptor,
) (*pb.UnitTestRunStatus, error) {
	testRes, err := s.server.RunTest(desc.FilePath)
	if err != nil {
		return nil, status.Error(codes.Internal, "Internal server error")
	}

	res := &pb.UnitTestRunStatus{
		Result:      testRes.Status,
		QueueTimeNs: uint64(testRes.QueueTime),
		RunTimeNs:   uint64(testRes.RunTime),
		Output:      testRes.Output,
	}
	return res, nil
}

// Status returns information about the server.
func (s *pwTestServer) Status(
	ctx context.Context,
	_ *pb.Empty,
) (*pb.ServerStatus, error) {
	resp := &pb.ServerStatus{
		UptimeNs:    uint64(time.Since(s.server.startTime)),
		TestsPassed: s.server.testsPassed,
		TestsFailed: s.server.testsFailed,
	}

	return resp, nil
}
