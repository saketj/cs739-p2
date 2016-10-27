#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <grpc++/grpc++.h>

#include "nfs.grpc.pb.h"
#include "nfs_server_utilities.h"
#include "nfs_server_batch_optimizer.h"

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
using nfs::SETATTRargs;
using nfs::SETATTRres;
using nfs::SETATTRresok;
using nfs::SETATTRresfail;
using nfs::READargs;
using nfs::READres;
using nfs::READresok;
using nfs::READresfail;
using nfs::WRITEargs;
using nfs::WRITEres;
using nfs::WRITEresok;
using nfs::WRITEresfail;
using nfs::COMMITargs;
using nfs::COMMITres;
using nfs::COMMITresok;
using nfs::COMMITresfail;
using nfs::CREATEargs;
using nfs::CREATEres;
using nfs::CREATEresok;
using nfs::CREATEresfail;
using nfs::REMOVEargs;
using nfs::REMOVEres;
using nfs::REMOVEresok;
using nfs::REMOVEresfail;
using nfs::MKDIRargs;
using nfs::MKDIRres;
using nfs::MKDIRresok;
using nfs::MKDIRresfail;
using nfs::RMDIRargs;
using nfs::RMDIRres;
using nfs::RMDIRresok;
using nfs::RMDIRresfail;
using nfs::LOOKUPargs;
using nfs::LOOKUPres;
using nfs::LOOKUPresok;
using nfs::LOOKUPresfail;
  

static const std::string SERVER_VERF = std::to_string(std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
static BatchWriteOptimizer batchWriteOptimizer;

class NFSServiceImpl final : public NFS::Service {
  Status NFSPROC_GETATTR(ServerContext* context, const GETATTRargs* getAttrArgs,
		         GETATTRres* getAttrRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(getAttrArgs->object()));
    if(server_path == NULL) {
      return Status::OK; 
    }
    
    struct stat sb;
    int res = lstat(server_path->c_str(), &sb);
    if (res == -1) { 
      getAttrRes->mutable_resok();
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

  Status NFSPROC_SETATTR(ServerContext* context, const SETATTRargs* setAttrArgs,
		         SETATTRres* setAttrRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(setAttrArgs->object()));
     if(server_path == NULL)
    {
      //getAttrRes->mutable_resok();
      return Status::OK;
    }

    int res = truncate(server_path->c_str(), setAttrArgs->new_attributes().size());
    if (res == -1) {
      return Status::OK;  // Failed to get attributes for the file.
    } else {
      setAttrRes->mutable_resok();
      return Status::OK;
    }
  }

  Status NFSPROC_READ(ServerContext* context, const READargs* readArgs,
		      READres* readRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(readArgs->file()));
     if(server_path == NULL)
    {
	readRes->mutable_resfail();
      //getAttrRes->mutable_resok();
      return Status::OK;
    }

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
		       WRITEres* writeRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(writeArgs->file()));
       if(server_path == NULL)
    {
        writeRes->mutable_resfail();
      //getAttrRes->mutable_resok();
      return Status::OK;
    }

    int fd = open(server_path->c_str(), O_WRONLY);
    if (fd == -1) {
      writeRes->mutable_resfail();
      return Status::OK;
    } else {
      if (writeArgs->stable() == WRITEargs::UNSTABLE) {
	// Unstable, fast, uncommitted writes with no fsync.
	const char *buf = writeArgs->data().c_str();
	batchWriteOptimizer.createRequest(writeArgs->file().data(), writeArgs->offset(), writeArgs->count(), buf);
	size_t bytes_written = writeArgs->count(); //pwrite(fd, buf, writeArgs->count(), writeArgs->offset());
	writeRes->mutable_resok()->set_count(bytes_written);
	writeRes->mutable_resok()->set_verf(SERVER_VERF);
	writeRes->mutable_resok()->set_committed(WRITEresok::UNSTABLE);
	close(fd);
	return Status::OK;
      } else {
	// Stable, slow, committed writes with forced fsync.
	const char *buf = writeArgs->data().c_str();
	#ifdef DEBUG
	std::cout << "Stable data: " << buf << std::endl;
	#endif
	size_t bytes_written = pwrite(fd, buf, writeArgs->count(), writeArgs->offset());
	writeRes->mutable_resok()->set_count(bytes_written);
	writeRes->mutable_resok()->set_verf(SERVER_VERF);
	fsync(fd);
	close(fd);
	writeRes->mutable_resok()->set_committed(WRITEresok::DATA_SYNC);
	return Status::OK;
      }
    }
  }

   Status NFSPROC_LOOKUP(ServerContext* context, const LOOKUPargs* lookupArgs,
                         LOOKUPres* lookupRes) override {
    std::unique_ptr<const std::string> server_path(getPathName(lookupArgs->what().dir()));
    struct stat sb;
    int res = lstat(server_path->c_str(), &sb);
    
    if (res == -1) {
      return Status::OK;  // Failed to get attributes for the file.
    } else {
	long inode_no = (long) sb.st_ino;
	std::string inode_str = std::to_string(inode_no);
	lookupRes->mutable_resok()->mutable_object()->set_data(inode_str.c_str()); 	
	return Status::OK;
    }
  }

  Status NFSPROC_COMMIT(ServerContext* context, const COMMITargs* commitArgs,
			COMMITres* commitRes) override {
    BatchWriteStatus status = batchWriteOptimizer.commitRequestFor(commitArgs->file().data(), commitArgs->offset(), commitArgs->count());
    if (status == BatchWriteStatus::kCommitSuccess || status == BatchWriteStatus::kCommitNone) {
      commitRes->mutable_resok();
      commitRes->mutable_resok()->set_verf(SERVER_VERF);
    } else {
      commitRes->mutable_resfail();
    }
    return Status::OK;
  }


  Status NFSPROC_MKDIR(ServerContext* context, const MKDIRargs* mkdirArgs,
                      MKDIRres* mkdirRes) override {

    std::unique_ptr<const std::string> server_path(getPathName(mkdirArgs->where().dir()));
    if(server_path == nullptr) {
        mkdirRes->mutable_resfail();
      //getAttrRes->mutable_resok();
      return Status::OK;
    }

    struct stat sb;
    if (stat(server_path->c_str(), &sb) == -1) {
        // Dir does not exist
	if(mkdir(server_path->c_str(), mkdirArgs->attributes().mode().mode()) == 0) { 
	  mkdirRes->mutable_resok(); 
	  return Status::OK;
	}
    }
    // if either else did not pass, its a failure
    mkdirRes->mutable_resfail();
    return Status::OK;
  }

  Status NFSPROC_RMDIR(ServerContext* context, const RMDIRargs* rmdirArgs,
                      RMDIRres* rmdirRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(rmdirArgs->object().dir()));
    if(server_path == nullptr) {
      rmdirRes->mutable_resfail();
      return Status::OK;
    }

    struct stat sb;
    if (stat(server_path->c_str(), &sb) != -1) {
        if(rmdir(server_path->c_str()) == 0) {
	  // Directory deleted
          rmdirRes->mutable_resok();
	  return Status::OK;
    	}
    }

    // Directory does not exist or rmdir failed
    rmdirRes->mutable_resfail();
    return Status::OK;
  }


  Status NFSPROC_CREATE(ServerContext* context, const CREATEargs* createArgs,
                        CREATEres* createRes) override {
    std::unique_ptr<const std::string> server_path(getPathName(createArgs->where().dir()));
    struct stat sb;
    if (stat(server_path->c_str(), &sb) == -1) {
      int fd = open(server_path->c_str(), O_CREAT, S_IRWXU | S_IRWXG);
      if (fd != -1) {         
	createRes->mutable_resok();
	close(fd);
	return Status::OK;
      } else {
	// File creation failed!
        createRes->mutable_resfail();
	return Status::OK;
      }
    } else {
      // File already exists at server!
      createRes->mutable_resok();
      return Status::OK;
    }
  }

  Status NFSPROC_REMOVE(ServerContext* context, const REMOVEargs* removeArgs,
                      REMOVEres* removeRes) override {
    std::unique_ptr<const std::string> server_path(getServerPath(removeArgs->object().dir()));
    if(server_path == nullptr) {
      removeRes->mutable_resfail();
      return Status::OK;
    }

    struct stat sb;
    if (stat(server_path->c_str(), &sb) != -1) {
        if(remove(server_path->c_str()) == 0) {
	  removeRes->mutable_resok();
	  return Status::OK;
        }
    }
    removeRes->mutable_resfail();
    return Status::OK;
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
  std::cout << "Server (ID: " << SERVER_VERF << ") listening on " << server_address <<  std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

void* RunBatchOptimizerThread(void *args) {
  #ifdef DEBUG
  std::cout << "Started Batch Optimizer Thread in background.\n";
  #endif
  long sleep_time_in_microseconds = 10000;  // every 10 ms we flush buffer in memory to disk.
  while(1) {
    usleep(sleep_time_in_microseconds);
    batchWriteOptimizer.scheduledCommit();
  }
  return nullptr;
}

int main(int argc, char** argv) {
  // Create and run BatchOptimizerThread.
  pthread_t batch_optimizer_thread;
  pthread_attr_t attr;
  pthread_attr_setschedpolicy(&attr, SCHED_IDLE);
  sched_param param;
  param.sched_priority = SCHED_IDLE;
  pthread_attr_setschedparam(&attr, &param);

  if (pthread_create(&batch_optimizer_thread, nullptr, RunBatchOptimizerThread, &attr)) {
    fprintf(stderr, "Error creating Batch Optimizer Thread\n");
    return 1;
  }
  
  RunServer();
  return 0;
}
