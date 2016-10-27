#ifndef _NFS_SERVER_UTILITIES_H_
#define _NFS_SERVER_UTILITIES_H_

#define SERVER_DATA_DIR "/tmp/nfs_server"
#define LAG_TIME 5  // Time in seconds that server lags before replying.
// #define DEBUG true

using nfs::nfs_fh;

const std::string* getServerPath(std::string fh_data) {
  std::unique_ptr<std::string> server_path(new std::string(std::string(SERVER_DATA_DIR) + fh_data));
  #ifdef DEBUG
  int sleep_time = rand() % LAG_TIME;
  std::cout << "server path " << *server_path << std::endl;
  std::cout << "Sleeping for " << sleep_time << " seconds.\n";
  sleep(sleep_time);
  #endif
  return server_path.release();
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
