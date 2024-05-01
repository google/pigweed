// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//	https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
//
// Package integration_test implements a client to exercise the pw_grpc server implementation
package integration_test

import (
	"bufio"
	"context"
	"fmt"
	"hash/crc32"
	"io"
	"os/exec"
	"strconv"
	"strings"
	"testing"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	pb "google.golang.org/grpc/examples/features/proto/echo"
	"google.golang.org/grpc/status"
)

const port = "3402"

func TestUnaryEcho(t *testing.T) {
	const num_connections = 1
	cmd, reader, err := launchServer(t, num_connections)
	if err != nil {
		t.Errorf("Failed to launch %v", err)
	}
	defer cmd.Wait()

	conn, echo_client, err := connectServer()
	if err != nil {
		t.Errorf("Failed to connect %v", err)
	}
	defer conn.Close()
	go logServer(t, reader)

	testRPC(t, func(t *testing.T, ctx context.Context, msg string) {
		t.Logf("call UnaryEcho(%v)", msg)
		resp, err := echo_client.UnaryEcho(ctx, &pb.EchoRequest{Message: msg})
		if err != nil {
			t.Logf("... failed with error: %v", err.Error())
			if msg != "quiet" || status.Convert(err).Code() != codes.Canceled {
				t.Errorf("Error unexpected %v", err)
			}
		} else {
			t.Logf("... Recv %v", resp)
			if resp.Message != msg {
				t.Errorf("Unexpected response %v", resp)
			}
		}
	})
}

func TestFragmentedMessage(t *testing.T) {
	// Test sending successively larger messages, larger than the maximum
	// HTTP2 data frame size (16384), ensuring messages are fragmented across
	// frames.
	const num_connections = 1
	cmd, reader, err := launchServer(t, num_connections)
	if err != nil {
		t.Errorf("Failed to launch %v", err)
	}
	defer cmd.Wait()

	conn, echo_client, err := connectServer()
	if err != nil {
		t.Errorf("Failed to connect %v", err)
	}
	defer conn.Close()
	go logServer(t, reader)

	const num_calls = 4
	for i := 0; i < num_calls; i++ {
		t.Run(fmt.Sprintf("%d of %d", i+1, num_calls), func(t *testing.T) {
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()

			msg := "crc32:" + strings.Repeat("testmessage!", 1500*(i+1))
			checksum := strconv.FormatUint(uint64(crc32.ChecksumIEEE([]byte(msg))), 10)

			done := make(chan struct{})
			go func() {
				t.Logf("call UnaryChecksum")
				resp, err := echo_client.UnaryEcho(ctx, &pb.EchoRequest{Message: msg})
				if err != nil {
					t.Logf("... failed with error: %v", err.Error())
					if msg != "quiet" || status.Convert(err).Code() != codes.Canceled {
						t.Errorf("Error unexpected %v", err)
					}
				} else {
					t.Logf("... Recv %v", resp)
					if resp.Message != checksum {
						t.Errorf("Unexpected response %v", resp)
					}
				}
				close(done)
			}()
			<-done
		})
	}
}

func TestMultipleConnections(t *testing.T) {
	const num_connections = 3
	cmd, reader, err := launchServer(t, num_connections)
	if err != nil {
		t.Errorf("Failed to launch %v", err)
	}
	defer cmd.Wait()

	go logServer(t, reader)

	for i := 0; i < num_connections; i++ {
		t.Run(fmt.Sprintf("connection %d of %d", i+1, num_connections), func(t *testing.T) {
			conn, echo_client, err := connectServer()
			if err != nil {
				t.Errorf("Failed to connect %v", err)
			}

			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()

			resp, err := echo_client.UnaryEcho(ctx, &pb.EchoRequest{Message: "message0"})
			if err != nil {
				t.Errorf("... failed with error: %v", err.Error())
			} else {
				t.Logf("... Recv %v", resp)
				if resp.Message != "message0" {
					t.Errorf("Unexpected response %v", resp)
				}
			}

			conn.Close()
		})
	}
}

func TestServerStreamingEcho(t *testing.T) {
	const num_connections = 1
	cmd, reader, err := launchServer(t, num_connections)
	if err != nil {
		t.Errorf("Failed to launch %v", err)
	}
	defer cmd.Wait()

	conn, echo_client, err := connectServer()
	if err != nil {
		t.Errorf("Failed to connect %v", err)
	}
	defer conn.Close()
	go logServer(t, reader)

	testRPC(t, func(t *testing.T, ctx context.Context, msg string) {
		t.Logf("call ServerStreamingEcho(%v)", msg)
		client, err := echo_client.ServerStreamingEcho(ctx, &pb.EchoRequest{Message: msg})
		if err != nil {
			t.Errorf("... failed with error: %v", err)
			return
		}
		for {
			resp, err := client.Recv()
			if err == io.EOF {
				t.Logf("... completed")
				return
			}
			if err != nil {
				t.Logf("... Recv failed with error: %v", err)
				if msg != "quiet" || status.Convert(err).Code() != codes.Canceled {
					t.Errorf("Error unexpected %v", err)
				}
				return
			}
			t.Logf("... Recv %v", resp)
			if resp.Message != msg && resp.Message != "done" {
				t.Errorf("Unexpected response %v", resp)
			}
		}
	})
}

func TestClientStreamingEcho(t *testing.T) {
	const num_connections = 1
	cmd, reader, err := launchServer(t, num_connections)
	if err != nil {
		t.Errorf("Failed to launch %v", err)
	}
	defer cmd.Wait()

	conn, echo_client, err := connectServer()
	if err != nil {
		t.Errorf("Failed to connect %v", err)
	}
	defer conn.Close()
	go logServer(t, reader)

	testRPC(t, func(t *testing.T, ctx context.Context, msg string) {
		t.Logf("call ClientStreamingEcho()")
		client, err := echo_client.ClientStreamingEcho(ctx)
		if err != nil {
			t.Errorf("... failed with error: %v", err)
			return
		}
		for i := 0; i < 3; i++ {
			t.Logf("... Send %v", msg)
			if err := client.Send(&pb.EchoRequest{Message: msg}); err != nil {
				t.Errorf("... Send failed with error: %v", err)
				return
			}
		}
		if err := client.CloseSend(); err != nil {
			t.Errorf("... CloseSend failed with error: %v", err)
			return
		}
		resp, err := client.CloseAndRecv()
		if err != nil {
			t.Logf("... CloseAndRecv failed with error: %v", err)
			if msg != "quiet" || status.Convert(err).Code() != codes.Canceled {
				t.Errorf("Error unexpected %v", err)
			}
		} else {
			t.Logf("... CloseAndRecv %v", resp)
			if resp.Message != "done" {
				t.Errorf("Unexpected response %v", resp)
			}
		}
	})
}

func TestBidirectionalStreamingEcho(t *testing.T) {
	const num_connections = 1
	cmd, reader, err := launchServer(t, num_connections)
	if err != nil {
		t.Errorf("Failed to launch %v", err)
	}
	defer cmd.Wait()

	conn, echo_client, err := connectServer()
	if err != nil {
		t.Errorf("Failed to connect %v", err)
	}
	defer conn.Close()
	go logServer(t, reader)

	testRPC(t, func(t *testing.T, ctx context.Context, msg string) {
		t.Logf("call BidirectionalStreamingEcho()")
		client, err := echo_client.BidirectionalStreamingEcho(ctx)
		if err != nil {
			t.Logf("... failed with error: %v", err)
			return
		}
		for i := 0; i < 3; i++ {
			t.Logf("... Send %v", msg)
			if err := client.Send(&pb.EchoRequest{Message: msg}); err != nil {
				t.Errorf("... Send failed with error: %v", err)
				return
			}
		}
		if err := client.CloseSend(); err != nil {
			t.Logf("... CloseSend failed with error: %v", err)
			return
		}
		for {
			resp, err := client.Recv()
			if err == io.EOF {
				t.Logf("... completed")
				return
			}
			if err != nil {
				t.Logf("... Recv failed with error: %v", err)
				if msg != "quiet" || status.Convert(err).Code() != codes.Canceled {
					t.Errorf("Error unexpected %v", err)
				}
				return
			}
			t.Logf("... Recv %v", resp)
			if resp.Message != msg {
				t.Errorf("Unexpected response %v", resp)
			}
		}
	})
}

func logServer(t *testing.T, reader *bufio.Reader) {
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			break
		}
		t.Logf("SERVER: %v", line)
	}
}

func launchServer(t *testing.T, num_connections int) (*exec.Cmd, *bufio.Reader, error) {
	cmd := exec.Command("./test_pw_rpc_server", port, strconv.Itoa(num_connections))

	output, err := cmd.StdoutPipe()
	if err != nil {
		t.Errorf("Failed to get stdout of server %v", err)
		return nil, nil, err
	}

	if err := cmd.Start(); err != nil {
		t.Errorf("Failed to launch server %v", err)
		return nil, nil, err
	}

	reader := bufio.NewReader(output)
	for {
		line, _ := reader.ReadString('\n')
		if strings.Contains(line, "Accept") {
			break
		}
	}

	return cmd, reader, nil
}

func connectServer() (*grpc.ClientConn, pb.EchoClient, error) {
	addr := "localhost:" + port

	conn, err := grpc.Dial(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, nil, err
	}

	echo_client := pb.NewEchoClient(conn)
	return conn, echo_client, nil
}

func testRPC(t *testing.T, call func(t *testing.T, ctx context.Context, msg string)) {
	const num_calls = 30
	for i := 0; i < num_calls; i++ {
		t.Run(fmt.Sprintf("%d of %d", i+1, num_calls), func(t *testing.T) {
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()

			msg := fmt.Sprintf("message%d", i)
			if i == num_calls-1 {
				msg = "quiet"
			}

			done := make(chan struct{})
			go func() {
				call(t, ctx, msg)
				close(done)
			}()
			// Test cancellation. When we sent "quiet", the server won't echo anything
			// back and instead will hold onto the request. Sleep a bit to make sure
			// the server doesn't respond. Then cancel the request, which should
			// complete the RPC.
			if msg == "quiet" {
				time.Sleep(100 * time.Millisecond)
				cancel()
			}
			<-done
		})
	}
}
