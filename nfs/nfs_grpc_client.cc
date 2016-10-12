#include <iostream>
#include <memory>
#include <string>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <grpc++/grpc++.h>

#include "nfs.grpc.pb.h"
#include "nfs_grpc_client_wrapper.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using nfs::NFS;
using nfs::fattr;
using nfs::GETATTRargs;
using nfs::GETATTRres;
using nfs::READargs;
using nfs::READres;
using nfs::WRITEargs;
using nfs::WRITEres;

#define SERVER "localhost"

class NFSClient {
 public:
  NFSClient(std::shared_ptr<Channel> channel)
      : stub_(NFS::NewStub(channel)) {}

  int NFSPROC_GETATTR(const char *path, struct stat *stbuf) {
    // Data we are sending to the server.
    GETATTRargs getAttrArgs;
    getAttrArgs.mutable_object()->set_data(path);

    // Container for the data we expect from the server.
    GETATTRres getAttrRes;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->NFSPROC_GETATTR(&context, getAttrArgs, &getAttrRes);

    // Act upon its status.
    if (status.ok() && getAttrRes.has_resok()) {
      // Populate the stbuf data structure using the getAttrRes.
      switch(getAttrRes.resok().obj_attributes().type()) {
      case fattr::NFSDIR: stbuf->st_mode |= S_IFDIR; break;
      case fattr::NFSREG: stbuf->st_mode |= S_IFREG; break;
      default: break;
      }
      stbuf->st_size = getAttrRes.resok().obj_attributes().size();
      stbuf->st_ino = getAttrRes.resok().obj_attributes().fileid();
      stbuf->st_atime = getAttrRes.resok().obj_attributes().atime().seconds();
      stbuf->st_mtime = getAttrRes.resok().obj_attributes().mtime().seconds();
      stbuf->st_ctime = getAttrRes.resok().obj_attributes().ctime().seconds();
      return 0;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }


  // Assambles the client's payload, sends it and presents the response back
  // from the server.
  int NFSPROC_READ(const char *path, char *buf, size_t buf_size, size_t offset) {
    // Data we are sending to the server.
    READargs readArgs;
    readArgs.mutable_file()->set_data(path);
    readArgs.set_offset(offset);
    readArgs.set_count(buf_size);

    // Container for the data we expect from the server.
    READres readRes;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->NFSPROC_READ(&context, readArgs, &readRes);

    // Act upon its status.
    if (status.ok() && readRes.has_resok()) {
      std::string data = readRes.mutable_resok()->data();
      std::size_t data_size = readRes.mutable_resok()->count();
      strncpy(buf, data.c_str(), data_size);
      return data_size;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

 int NFSPROC_WRITE(const char *path, const char *buf, size_t buf_size, size_t offset) {
    // Data we are sending to the server.
    WRITEargs writeArgs;
    writeArgs.mutable_file()->set_data(path);
    writeArgs.set_offset(offset);
    writeArgs.set_count(buf_size);
    writeArgs.set_data(buf);

    // Container for the data we expect from the server.
    WRITEres writeRes;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->NFSPROC_WRITE(&context, writeArgs, &writeRes);

    // Act upon its status.
    if (status.ok() && writeRes.has_resok()) {
      std::size_t data_size = writeRes.mutable_resok()->count();      
      return data_size;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

 private:
  std::unique_ptr<NFS::Stub> stub_;
};

NFSClient* getNFSClient() {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  std::string connection = std::string(SERVER) + ":50051";  
  std::unique_ptr<NFSClient> nfs_client(new NFSClient(grpc::CreateChannel(connection, grpc::InsecureChannelCredentials())));
  return nfs_client.release();
}

int remote_getattr(const char *path, struct stat *stbuf) {
  std::unique_ptr<NFSClient> nfs_client(getNFSClient());
  int res = nfs_client->NFSPROC_GETATTR(path, stbuf);
  return res;
}

int remote_read(const char *path, char *buffer, size_t buffer_size, size_t offset) {
  std::unique_ptr<NFSClient> nfs_client(getNFSClient());
  int buffer_read = nfs_client->NFSPROC_READ(path, buffer, buffer_size, offset);
  return buffer_read;
}

int remote_write(const char *path, const char *buffer, const size_t buffer_size, const size_t offset) {
  std::unique_ptr<NFSClient> nfs_client(getNFSClient());
  int buffer_written = nfs_client->NFSPROC_WRITE(path, buffer, buffer_size, offset);
  return buffer_written;
}
