#include <iostream>
#include <memory>
#include <string>
#include <chrono>
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
#include <thread>
#include <unistd.h>
#include <unordered_map>

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
using nfs::SETATTRargs;
using nfs::SETATTRres;
using nfs::READargs;
using nfs::READres;
using nfs::WRITEargs;
using nfs::WRITEres;
using nfs::COMMITargs;
using nfs::COMMITres;

#define SERVER "localhost"
#define RPC_TIMEOUT 5000  // Timeout in milliseconds after which the rpc request will fail
#define CONN_TIMEOUT 100000 // Timeout in ms after which the client timeouts on the server
#define RETRY 100   // Retry the rpc request after these many milliseconds

static std::string latest_write_server_verf = "";
static std::unordered_map<std::string, std::vector<WRITEargs>> client_buffer_map;
  
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

    int retry_interval = RETRY;
    Status status; 
    do {
      // Context for the client. It could be used to convey extra information to
      // the server and/or tweak certain RPC behaviors.
      std::unique_ptr<ClientContext> context(getClientContext());
      // The actual RPC.
      status = stub_->NFSPROC_GETATTR(context.get(), getAttrArgs, &getAttrRes);
    } while (isRetryRequiredForStatus(status, retry_interval));

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
  
  int NFSPROC_SETATTR(const char *path, size_t size) {
    // Data we are sending to the server.
    SETATTRargs setAttrArgs;
    setAttrArgs.mutable_object()->set_data(path);
    setAttrArgs.mutable_new_attributes()->set_size(size);

    // Container for the data we expect from the server.
    SETATTRres setAttrRes;

    int retry_interval = RETRY;
    Status status;
    do {
      // Context for the client. It could be used to convey extra information to
      // the server and/or tweak certain RPC behaviors.
      std::unique_ptr<ClientContext> context(getClientContext());
      // The actual RPC.
      status = stub_->NFSPROC_SETATTR(context.get(), setAttrArgs, &setAttrRes);
    } while (isRetryRequiredForStatus(status, retry_interval));

    // Act upon its status.
    if (status.ok() && setAttrRes.has_resok()) {
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

    int retry_interval = RETRY;
    Status status;
    do {
      // Context for the client. It could be used to convey extra information to
      // the server and/or tweak certain RPC behaviors.
      // The actual RPC.
      std::unique_ptr<ClientContext> context(getClientContext());
      status = stub_->NFSPROC_READ(context.get(), readArgs, &readRes);
    } while (isRetryRequiredForStatus(status, retry_interval));

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

  int NFSPROC_WRITE(const char *path, const char *buf, size_t buf_size, size_t offset, bool isUnstable = true) {
    // Data we are sending to the server.
    WRITEargs writeArgs;
    writeArgs.mutable_file()->set_data(path);
    writeArgs.set_offset(offset);
    writeArgs.set_count(buf_size);
    writeArgs.set_data(buf);

    if (isUnstable) {
      writeArgs.set_stable(WRITEargs::UNSTABLE);
      // For unstable writes, we store the data in the client buffer.
      std::string path_str(path);
      if (client_buffer_map.find(path_str) == client_buffer_map.end()) {
	std::vector<WRITEargs> request_vec;
	client_buffer_map.insert(make_pair(path_str, request_vec));
      }
      // Create copy of the write data.
      client_buffer_map[path_str].push_back(writeArgs);
    } else {
      writeArgs.set_stable(WRITEargs::DATA_SYNC);
    }

    // Container for the data we expect from the server.
    WRITEres writeRes;

    int retry_interval = RETRY;
    Status status;
    do {
      // Context for the client. It could be used to convey extra information to
      // the server and/or tweak certain RPC behaviors.
      std::unique_ptr<ClientContext> context(getClientContext());
      // The actual RPC.
      status = stub_->NFSPROC_WRITE(context.get(), writeArgs, &writeRes);
    } while (isRetryRequiredForStatus(status, retry_interval));

    // Act upon its status.
    if (status.ok() && writeRes.has_resok()) {
      std::size_t data_size = writeRes.resok().count();
      latest_write_server_verf = writeRes.resok().verf();
      return data_size;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

  int NFSPROC_COMMIT(const char *path) {
    // Data we are sending to the server.
    COMMITargs commitArgs;
    commitArgs.mutable_file()->set_data(path);
    commitArgs.set_offset(0);  // Assumption: Entire file is synced.
    commitArgs.set_count(0);   // Assumption: Entire file is synced.
    
    // Check if we have any pending buffer to commit, at all.
    if (client_buffer_map.find(commitArgs.file().data()) == client_buffer_map.end()) {
      return 0; // nothing to commit, just return.
    }
    
    // Container for the data we expect from the server.
    COMMITres commitRes;

    int retry_interval = RETRY;
    Status status;
    do {
      // Context for the client. It could be used to convey extra information to
      // the server and/or tweak certain RPC behaviors.
      std::unique_ptr<ClientContext> context(getClientContext());
      // The actual RPC.
      status = stub_->NFSPROC_COMMIT(context.get(), commitArgs, &commitRes);
    } while (isRetryRequiredForStatus(status, retry_interval));

    // Act upon its status.
    if (status.ok() && commitRes.has_resok()) {
      return releaseBuffersBasedOnCommitStatus(commitArgs.file().data(), commitRes);
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }
 
  ClientContext* getClientContext() {
    std::unique_ptr<ClientContext> client_context(new ClientContext);
    std::chrono::system_clock::time_point deadline = 
      std::chrono::system_clock::now() + std::chrono::milliseconds(RPC_TIMEOUT);
    client_context->set_deadline(deadline);
    return client_context.release();
  }
 
  bool isRetryRequiredForStatus(const Status &status, int &retry_interval) {
    if (status.ok()) {
      return false;
    } else {
      // Wait for retry_interval to elapse.
      std::cout << "RPC timed out. Retrying after " << retry_interval << " ms.\n";
      std::this_thread::sleep_for (std::chrono::milliseconds(retry_interval));
      retry_interval *= 2;  // Exponential backoff.
      return true;
    }
  }
  
  int releaseBuffersBasedOnCommitStatus(const std::string &path, const COMMITres &commitRes) {
    if (latest_write_server_verf.compare(commitRes.resok().verf()) != 0) {
      printf("versions don't match\n");
      // Server has crashed and come back since the latest write, hence 
      // all pending uncommitted writes need to be retransmitted.
      std::vector<WRITEargs> &request_vec = client_buffer_map[path];
      for (WRITEargs &writeArgs : request_vec) {
	int res = NFSPROC_WRITE(writeArgs.file().data().c_str(), writeArgs.data().c_str(), writeArgs.count(), writeArgs.offset(), false);  // The write is stable;
	if (res < 0) return res;
      }
    }
    
    client_buffer_map.erase(path);  // Release the buffer.

    return 0;
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
  std::shared_ptr<Channel> channel = grpc::CreateChannel(connection, grpc::InsecureChannelCredentials());
  std::chrono::system_clock::time_point deadline = 
      std::chrono::system_clock::now() + std::chrono::milliseconds(CONN_TIMEOUT);
  channel->WaitForConnected(deadline);
  
  std::unique_ptr<NFSClient> nfs_client(new NFSClient(channel));    
  return nfs_client.release();
}

int remote_getattr(const char *path, struct stat *stbuf) {
  std::unique_ptr<NFSClient> nfs_client(getNFSClient());
  int res = nfs_client->NFSPROC_GETATTR(path, stbuf);
  return res;
}

int remote_setattr(const char *path, size_t size) {
  std::unique_ptr<NFSClient> nfs_client(getNFSClient());
  int res = nfs_client->NFSPROC_SETATTR(path, size);
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


int remote_fsync(const char *path) {
  printf("Sleeping in remote_fsync for 5 seconds.\n");
  sleep(5);
  std::unique_ptr<NFSClient> nfs_client(getNFSClient());
  int res = nfs_client->NFSPROC_COMMIT(path);
  return res;
}
