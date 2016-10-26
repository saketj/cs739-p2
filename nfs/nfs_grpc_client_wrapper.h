#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif
  int remote_setattr(const char *path, size_t size);
  int remote_getattr(const char *path, struct stat *stbuf);
  int remote_read(const char *path, char *buf, size_t buf_size, size_t offset);
  int remote_write(const char *path, const char *buf, size_t buf_size, size_t offset);
  int remote_fsync(const char *path);
  int remote_mkdir(const char *path, mode_t mode);
  int remote_rmdir(const char *path);  
  int remote_open(const char *path, mode_t mode);
  int remote_create(const char *path, int flags, mode_t mode);
  int remote_unlink(const char *path);
#ifdef __cplusplus
}
#endif
