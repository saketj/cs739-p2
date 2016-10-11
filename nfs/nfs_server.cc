/*
 *
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

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>

#include "nfs.grpc.pb.h"

#define SERVER_DATA_DIR "/tmp/nfs_server"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using nfs::NFS;
using nfs::nfs_fh;
using nfs::READargs;
using nfs::READres;
using nfs::READres;
using nfs::READresok;
using nfs::READresfail;

const std::string* getServerPath(nfs_fh file_handle) {
  std::unique_ptr<std::string> server_path(new std::string(std::string(SERVER_DATA_DIR) + file_handle.data()));
  return server_path.release();
}


class NFSServiceImpl final : public NFS::Service {
  Status NFSPROC_READ(ServerContext* context, const READargs* readArgs,
		      READres* readRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(readArgs->file()));
    FILE *file = fopen(server_path->c_str(), "rb");
    if (file == nullptr) {
      readRes->mutable_resfail();
      return Status::OK;
    } else {
      fseek(file, readArgs->offset(), SEEK_SET);
      std::unique_ptr<char> buf(new char[readArgs->count()]);
      size_t bytes_read = fread(buf.get(), 1, readArgs->count(), file);
      readRes->mutable_resok()->set_data(buf.get());
      readRes->mutable_resok()->set_count(bytes_read);
      return Status::OK;
    }
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  NFSServiceImpl service;
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
