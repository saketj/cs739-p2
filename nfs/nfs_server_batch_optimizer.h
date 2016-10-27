#ifndef _NFS_SERVER_BATCH_OPTIMIZER_H_
#define _NFS_SERVER_BATCH_OPTIMIZER_H_

#include <unordered_map>
#include <set>

#include "nfs_server_utilities.h"

#define SCHEDULED_BATCH_COMMIT_SIZE 1

using nfs::nfs_fh;

class BatchWriteRequest {
  friend class BatchWriteOptimizer;
 public:
  BatchWriteRequest(uint32_t request_id)
    :  request_id_(request_id) {
  }

  BatchWriteRequest(uint32_t request_id, std::string fh_data, size_t offset, size_t count, const char *buf)
     : request_id_(request_id),
       fh_data_(fh_data),
       offset_(offset),
       count_(count) {
     buf_.reset(new char[count]);
     memcpy(buf_.get(), buf, count); 
  }
 private:
  uint32_t request_id_;
  std::string fh_data_;
  size_t offset_;
  size_t count_;
  std::unique_ptr<char> buf_;
};

enum BatchWriteStatus {
  kCreateSuccess,
  kCommitSuccess,
  kCommitNone,
  kCreateFailure,
  kCommitFailure,
  kFail
};

class BatchWriteOptimizer {
 public:
  struct request_queue_comparator {
    bool operator() (const BatchWriteRequest& lhs, const BatchWriteRequest& rhs) const {
      return lhs.request_id_  < rhs.request_id_;
    }
  };
  
  BatchWriteOptimizer() {
    next_request_id = 0;
    pthread_mutex_init(&request_queue_mutex, nullptr);
  }
  
  BatchWriteStatus createRequest(std::string fh_data, size_t offset, size_t count, const char *buf) {
    BatchWriteRequest request(next_request_id++, fh_data, offset, count, buf);
    batch_write_request_queue.insert(std::move(request));
    
    if (fh_map.find(fh_data) == fh_map.end()) {
      std::vector<uint32_t> fh_ops;
      fh_map.insert(make_pair(fh_data, fh_ops));
    }

    fh_map[fh_data].push_back(request.request_id_);
    
    return BatchWriteStatus::kCreateSuccess;
  }
  
  BatchWriteStatus commitRequestFor(std::string fh_data, size_t offset, size_t count) {
    if (fh_map.find(fh_data) == fh_map.end()) {
      return BatchWriteStatus::kCommitNone;
    }
    
    // Acquire the lock and commit for this fh using the given offset and count.
    pthread_mutex_lock(&request_queue_mutex);

    std::vector<uint32_t> &fh_ops = fh_map[fh_data];
    for (uint32_t fh_op : fh_ops) {
      BatchWriteRequest key(fh_op);
      auto request = batch_write_request_queue.find(key);
      if (request != batch_write_request_queue.end()) {
	std::unique_ptr<const std::string> server_path(getServerPath(request->fh_data_));
	synchronous_write(server_path.get(), request->offset_, request->count_, request->buf_.get());
	batch_write_request_queue.erase(request);
      }
    }
    
    // Release the lock.
    pthread_mutex_unlock(&request_queue_mutex);

    return BatchWriteStatus::kCommitSuccess;
  }

  void scheduledCommit() {
    // Acquire the lock and commit the requests pending at the head of the queue.
    pthread_mutex_lock(&request_queue_mutex);

    for (int i = 0; i < SCHEDULED_BATCH_COMMIT_SIZE; ++i) {
      if (batch_write_request_queue.empty()) break;
      auto request = batch_write_request_queue.begin();
      #ifdef DEBUG
      std::cout << "Scheduled commit for: " << request->request_id_ << std::endl;
      #endif
      std::unique_ptr<const std::string> server_path(getServerPath(request->fh_data_));
      synchronous_write(server_path.get(), request->offset_, request->count_, request->buf_.get());
      batch_write_request_queue.erase(request);
    }

    // Release the lock.
    pthread_mutex_unlock(&request_queue_mutex);
  }
 
 private:
  uint32_t next_request_id;
  std::unordered_map<std::string, std::vector<uint32_t>> fh_map;
  std::set<BatchWriteRequest, request_queue_comparator> batch_write_request_queue;  // A sorted queue based on req_id
  pthread_mutex_t request_queue_mutex;
};



#endif // _NFS_SERVER_BATCH_OPTIMIZER_H_
