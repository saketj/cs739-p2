#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

  int remote_read(const char *path, char *buf, size_t buf_size, size_t offset);

#ifdef __cplusplus
}
#endif
