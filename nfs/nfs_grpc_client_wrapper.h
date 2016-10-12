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
  
#ifdef __cplusplus
}
#endif
