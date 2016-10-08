/*
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <algorithm>

#include <grpc++/grpc++.h>

#include "nfs.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using nfs::HelloRequest;
using nfs::HelloReply;
using nfs::Greeter;


#define SERVER "localhost"
#define BILLION 1000000000L
#define NUM_ITERATIONS 10
#define BUFFER_SIZE 64000

class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assambles the client's payload, sends it and presents the response back
  // from the server.
  char SayHello(int *arr, int size) {
    // Data we are sending to the server.
    HelloRequest request;
    for (int i = 0; i < size; ++i) {
    	request.add_data(arr[i]);
    }


    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return '0';
    }
  }

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

double findMedian(std::vector<uint64_t> &vec) {
  std::sort(vec.begin(), vec.end());
  int size = vec.size();
  if (size % 2 == 0) {
    double val = (double)(vec[size/2] + vec[size/2 + 1]) / (double)2.0f;
    return val;
  }
  else {
    return vec[size/2];
  }
}

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  std::string connection = std::string(SERVER) + ":50051";
  GreeterClient greeter(grpc::CreateChannel(connection, grpc::InsecureChannelCredentials()));

  uint64_t total_time = 0;
  struct timespec start, end;
  int size = 16000;
  char message[BUFFER_SIZE];

  if (argc < 2) {
    printf("Incorrect invocation. Correct usage: ./client <file>\n");
    exit(1);
  }

  char *filename = argv[1];
  FILE *fp;

  std::vector<uint64_t> results(NUM_ITERATIONS, 0);
  for (int itr = 0; itr < NUM_ITERATIONS; ++itr) {
    // Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
      fprintf(stderr, "Can't open the input file!\n");
      exit(1);
    }
    uint64_t iter_time = 0;
    // Read and send the entire file, one buffer at a time
    while (fread(message, 1, sizeof(message), fp) > 0) {
      int *data = (int *) message;
      int size = BUFFER_SIZE / 4;

      clock_gettime(CLOCK_REALTIME, &start);/* mark start time */
      char reply = greeter.SayHello(data, size);
      clock_gettime(CLOCK_REALTIME, &end);/* mark end time */

      iter_time += BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    }
    results[itr] = iter_time;
    fclose(fp);
  }
  printf("Median time taken = %f nanoseconds for %s data.\n", findMedian(results), filename);

  return 0;
}
