#ifndef _NFS_SERVER_UTILITIES_H_
#define _NFS_SERVER_UTILITIES_H_

#define SERVER_DATA_DIR "/tmp/nfs_server"
#define LAG_TIME 5  // Time in seconds that server lags before replying.

// #define DEBUG true


#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

using nfs::nfs_fh;

static const std::string SERVER_DATA_DIR_STR = std::string(SERVER_DATA_DIR);

const std::string* getPathName(std::string fh_data) {
  std::unique_ptr<std::string> server_path(new std::string(std::string(SERVER_DATA_DIR) + fh_data));
  #ifdef DEBUG
  int sleep_time = rand() % LAG_TIME;
  std::cout << "server path " << *server_path << std::endl;
  std::cout << "Sleeping for " << sleep_time << " seconds.\n";
  sleep(sleep_time);
  #endif
  return server_path.release();
}

const char* inode_path(std::string path, int level, long inode_no)
{
  struct stat path_stat;
  int res = lstat(path.c_str(), &path_stat);
  if (res == -1) return nullptr;  // End-of-Recursion
  
  long curr_inode = (long) path_stat.st_ino;
  if (curr_inode == inode_no) {
      return path.c_str();  // Inode found, End-of-Recursion!
  }
  
  if (S_ISREG(path_stat.st_mode)) {
    // Regular File detected, End-of-Recursion 
    return nullptr;
  }
  
  DIR *dir;
  dirent *entry;
    
  if (!(dir = opendir(path.c_str()))) return nullptr;

  if (!(entry = readdir(dir))) return nullptr;

  do {
    const char *d_name = entry->d_name;
    if (strcmp(d_name, ".") != 0 && strcmp(d_name, "..") != 0) {
      std::string r_path = path + "/" + std::string(d_name);
      const char* result = inode_path(r_path, level + 1, inode_no);
      if (result != nullptr) {
	closedir(dir);
	return result;
      }
    }
  } while (entry = readdir(dir));
  closedir(dir);
  return nullptr;
}

const std::string* getPathName(nfs_fh file_handle) {
  return getPathName(file_handle.data());
}


const std::string* getServerPath(std::string fh_data) {
  long inode_no = atol(fh_data.c_str());
  const char *ret_path = inode_path(SERVER_DATA_DIR_STR, 0, inode_no);
  if (ret_path == nullptr) {
    std::unique_ptr<std::string> server_path(nullptr);
    return server_path.release();
  }
  else {
    std::unique_ptr<std::string> server_path(new std::string(ret_path));
    return server_path.release();
  }
}

const std::string* getServerPath(nfs_fh file_handle) {
  return getServerPath(file_handle.data());
}

int synchronous_write(const std::string *server_path, size_t offset, size_t count, const char *buf) {
    int fd = open(server_path->c_str(), O_WRONLY);
    if (fd == -1) {
      return -1;
    } else {
      size_t bytes_written = pwrite(fd, buf, count, offset);
      fsync(fd);
      close(fd);
      return bytes_written;
    }
  }


#endif  // _NFS_SERVER_UTILITIES_H_
