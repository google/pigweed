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
package client

import (
	"context"
	"errors"
	"fmt"
	"path/filepath"
	"time"

	"google.golang.org/grpc"

	pb "pigweed.dev/module/pw_test_server/gen"
)

// Client is a gRPC client that communicates with a TestServer service.
type Client struct {
	conn *grpc.ClientConn
}

// New creates a gRPC client which connects to a gRPC server hosted at the
// specified address.
func New(host string, port int) (*Client, error) {
	// The server currently only supports running locally over an insecure
	// connection.
	// TODO(frolv): Investigate adding TLS support to the server and client.
	opts := []grpc.DialOption{grpc.WithInsecure()}

	conn, err := grpc.Dial(fmt.Sprintf("%s:%d", host, port), opts...)
	if err != nil {
		return nil, err
	}

	return &Client{conn}, nil
}

// RunTest sends a RunUnitTest RPC to the test server.
func (c *Client) RunTest(path string) error {
	abspath, err := filepath.Abs(path)
	if err != nil {
		return err
	}

	client := pb.NewTestServerClient(c.conn)
	req := &pb.UnitTestDescriptor{FilePath: abspath}

	res, err := client.RunUnitTest(context.Background(), req)
	if err != nil {
		return err
	}

	fmt.Printf("%s\n", path)
	fmt.Printf(
		"Queued for %v, ran in %v\n\n",
		time.Duration(res.QueueTimeNs),
		time.Duration(res.RunTimeNs),
	)
	fmt.Println(string(res.Output))

	if res.Result != pb.TestStatus_SUCCESS {
		return errors.New("Unit test failed")
	}

	return nil
}
