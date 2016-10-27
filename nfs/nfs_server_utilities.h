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

const char* inode_path(const char *name, int level, long inode_no)
{
    DIR *dir;
    struct dirent *entry;
    //std::cout << "Inode to path is getting called\n"; 
    if (!(dir = opendir(name)))
        return NULL;
    if (!(entry = readdir(dir)))
        return NULL;
    //std::cout << "Pathname of root: " << name << " inode_no: " << inode_no << "\n";
    do {
        //std::cout<< "Name: " << entry->d_name << "\n"; 
        if (entry->d_type == DT_DIR) { 
            char path[1024];
            int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
            path[len] = 0;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            //printf("%*s[%s]\n", level*2, "", entry->d_name);
            inode_path(path, level + 1, inode_no);
        }
        //else
	//{
	    struct stat sb;
	    std::unique_ptr<std::string> path(new std::string(std::string(SERVER_DATA_DIR) + "/" +entry->d_name)); 
	    //std::cout << "New path name: " << std::string(path->c_str()) << "\n"; 
	    int res = lstat(path->c_str(), &sb);
	    if (res == -1)
	   {
		return NULL; 
	   }
	    long inode_no_current = (long) sb.st_ino;
	    if(inode_no_current == inode_no)
	    {
		//std::cout << "Yay\n"; 
		return path->c_str(); 
	    }
            //printf("%*s- %s\n", level*2, "", entry->d_name);
	    //}
    } while (entry = readdir(dir));
    //std::cout << "Out of while loop \n"; 
    //closedir(dir);
	return NULL; 
}

const std::string* getPathName(nfs_fh file_handle) {
  return getPathName(file_handle.data());
}


const std::string* getServerPath(std::string fh_data) {
  long inode_no = atol (fh_data.c_str()); 
  std::cout << "Inode which is going to be mapped to path: " << inode_no << "\n"; 
  std::unique_ptr<std::string> server_path(new std::string(inode_path(std::string(SERVER_DATA_DIR).c_str(), 0, inode_no)));
  std::cout << "server path mapped from inode: " << *server_path << std::endl;

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
