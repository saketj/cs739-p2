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
#include <cstring>
#include <vector>
#include <algorithm>

#include <grpc++/grpc++.h>

#include "nfs.grpc.pb.h"
#include "nfs_grpc_client_wrapper.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using nfs::NFS;
using nfs::READargs;
using nfs::READres;

#define SERVER "localhost"

class NFSClient {
 public:
  NFSClient(std::shared_ptr<Channel> channel)
      : stub_(NFS::NewStub(channel)) {}

  // Assambles the client's payload, sends it and presents the response back
  // from the server.
  std::string NFSPROC_READ() {
    // Data we are sending to the server.
    READargs readArgs;
    readArgs.set_nfs_fh("random:file:handle");
    readArgs.set_offset(0);
    readArgs.set_count(100);

    // Container for the data we expect from the server.
    READres readRes;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->NFSPROC_READ(&context, readArgs, &readRes);

    // Act upon its status.
    if (status.ok()) {
      return readRes.mutable_resok()->data();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "0";
    }
  }

 private:
  std::unique_ptr<NFS::Stub> stub_;
};

void remote_read(char *buffer, int buffer_size) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  std::string connection = std::string(SERVER) + ":50051";
  NFSClient nfs_client(grpc::CreateChannel(connection, grpc::InsecureChannelCredentials()));
  std::string reply = nfs_client.NFSPROC_READ();
  strncpy(buffer, reply.c_str(), buffer_size);
}
