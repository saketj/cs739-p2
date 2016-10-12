#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <grpc++/grpc++.h>

#include "nfs.grpc.pb.h"

#define SERVER_DATA_DIR "/tmp/nfs_server"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using nfs::NFS;
using nfs::nfs_fh;
using nfs::fattr;
using nfs::fattr_ftype;
using nfs::GETATTRargs;
using nfs::GETATTRres;
using nfs::GETATTRresok;
using nfs::READargs;
using nfs::READres;
using nfs::READresok;
using nfs::READresfail;
using nfs::WRITEargs;
using nfs::WRITEres;
using nfs::WRITEresok;
using nfs::WRITEresfail;

const std::string* getServerPath(nfs_fh file_handle) {
  std::unique_ptr<std::string> server_path(new std::string(std::string(SERVER_DATA_DIR) + file_handle.data()));
  return server_path.release();
}


class NFSServiceImpl final : public NFS::Service {
  Status NFSPROC_GETATTR(ServerContext* context, const GETATTRargs* getAttrArgs,
		         GETATTRres* getAttrRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(getAttrArgs->object()));
    struct stat sb;
    int res = lstat(server_path->c_str(), &sb);
    if (res == -1) {
      return Status::OK;  // Failed to get attributes for the file.
    } else {
      // Populate fattr based on stat.
      switch(sb.st_mode & S_IFMT) {
      case S_IFDIR: getAttrRes->mutable_resok()->mutable_obj_attributes()->set_type(fattr::NFSDIR); break;
      case S_IFREG: getAttrRes->mutable_resok()->mutable_obj_attributes()->set_type(fattr::NFSREG); break;
      default: break;
      }
      getAttrRes->mutable_resok()->mutable_obj_attributes()->set_size(sb.st_size);
      getAttrRes->mutable_resok()->mutable_obj_attributes()->set_fileid(sb.st_ino);
      getAttrRes->mutable_resok()->mutable_obj_attributes()->mutable_atime()->set_seconds(sb.st_atime);
      getAttrRes->mutable_resok()->mutable_obj_attributes()->mutable_mtime()->set_seconds(sb.st_mtime);
      getAttrRes->mutable_resok()->mutable_obj_attributes()->mutable_ctime()->set_seconds(sb.st_ctime);
      return Status::OK;
    }
  }  

  Status NFSPROC_READ(ServerContext* context, const READargs* readArgs,
		      READres* readRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(readArgs->file()));
    int fd = open(server_path->c_str(), O_RDONLY);
    if (fd == -1) {
      readRes->mutable_resfail();
      return Status::OK;
    } else {
      std::unique_ptr<char> buf(new char[readArgs->count()]);
      size_t bytes_read = pread(fd, buf.get(), readArgs->count(), readArgs->offset());
      readRes->mutable_resok()->set_data(buf.get());
      readRes->mutable_resok()->set_count(bytes_read);
      close(fd);
      return Status::OK;
    }
  }

  Status NFSPROC_WRITE(ServerContext* context, const WRITEargs* writeArgs,
		       WRITEres* readRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(writeArgs->file()));
    int fd = open(server_path->c_str(), O_WRONLY);
    if (fd == -1) {
      readRes->mutable_resfail();
      return Status::OK;
    } else {
      const char *buf = writeArgs->data().c_str();
      size_t bytes_written = pwrite(fd, buf, writeArgs->count(), writeArgs->offset());
      readRes->mutable_resok()->set_count(bytes_written);
      close(fd);
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
