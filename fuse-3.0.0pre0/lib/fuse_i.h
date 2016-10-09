/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse.h"
#include "fuse_lowlevel.h"

struct fuse_chan;
struct fuse_ll;
struct mount_opts;

struct fuse_session {
	struct fuse_ll *f;
	char *mountpoint;

	volatile int exited;

	struct fuse_chan *ch;
	struct mount_opts *mo;
};

struct fuse_chan {
	struct fuse_session *se;

	pthread_mutex_t lock;
	int ctr;
	int fd;
};


struct fuse_req {
	struct fuse_ll *f;
	uint64_t unique;
	int ctr;
	pthread_mutex_t lock;
	struct fuse_ctx ctx;
	struct fuse_chan *ch;
	int interrupted;
	unsigned int ioctl_64bit : 1;
	union {
		struct {
			uint64_t unique;
		} i;
		struct {
			fuse_interrupt_func_t func;
			void *data;
		} ni;
	} u;
	struct fuse_req *next;
	struct fuse_req *prev;
};

struct fuse_notify_req {
	uint64_t unique;
	void (*reply)(struct fuse_notify_req *, fuse_req_t, fuse_ino_t,
		      const void *, const struct fuse_buf *);
	struct fuse_notify_req *next;
	struct fuse_notify_req *prev;
};

struct fuse_ll {
	int debug;
	int allow_root;
	int atomic_o_trunc;
	int no_remote_posix_lock;
	int no_remote_flock;
	int big_writes;
	int splice_write;
	int splice_move;
	int splice_read;
	int no_splice_write;
	int no_splice_move;
	int no_splice_read;
	int auto_inval_data;
	int no_auto_inval_data;
	int no_readdirplus;
	int no_readdirplus_auto;
	int async_dio;
	int no_async_dio;
	int writeback_cache;
	int no_writeback_cache;
	int clone_fd;
	struct fuse_lowlevel_ops op;
	int got_init;
	struct cuse_data *cuse_data;
	void *userdata;
	uid_t owner;
	struct fuse_conn_info conn;
	struct fuse_req list;
	struct fuse_req interrupts;
	pthread_mutex_t lock;
	int got_destroy;
	pthread_key_t pipe_key;
	int broken_splice_nonblock;
	uint64_t notify_ctr;
	struct fuse_notify_req notify_list;
	size_t bufsize;
};

/**
 * Filesystem module
 *
 * Filesystem modules are registered with the FUSE_REGISTER_MODULE()
 * macro.
 *
 */
struct fuse_module {
	char *name;
	fuse_module_factory_t factory;
	struct fuse_module *next;
	struct fusemod_so *so;
	int ctr;
};

int fuse_chan_clearfd(struct fuse_chan *ch);
void fuse_chan_close(struct fuse_chan *ch);

/* ----------------------------------------------------------- *
 * Channel interface					       *
 * ----------------------------------------------------------- */

 /**
 * Create a new channel
 *
 * @param op channel operations
 * @param fd file descriptor of the channel
 * @return the new channel object, or NULL on failure
 */
struct fuse_chan *fuse_chan_new(int fd);

/**
 * Query the session to which this channel is assigned
 *
 * @param ch the channel
 * @return the session, or NULL if the channel is not assigned
 */
struct fuse_session *fuse_chan_session(struct fuse_chan *ch);

/**
 * Remove the channel from a session
 *
 * If the channel is not assigned to a session, then this is a no-op
 *
 * @param ch the channel to remove
 */
void fuse_session_remove_chan(struct fuse_chan *ch);

/**
 * Assign a channel to a session
 *
 * If a session is destroyed, the assigned channel is also destroyed
 *
 * @param se the session
 * @param ch the channel
 */
void fuse_session_add_chan(struct fuse_session *se, struct fuse_chan *ch);

/**
 * Return channel assigned to the session
 *
 * @param se the session
 * @return the channel
 */
struct fuse_chan *fuse_session_chan(struct fuse_session *se);

/**
 * Obtain counted reference to the channel
 *
 * @param ch the channel
 * @return the channel
 */
struct fuse_chan *fuse_chan_get(struct fuse_chan *ch);

/**
 * Drop counted reference to a channel
 *
 * @param ch the channel
 */
void fuse_chan_put(struct fuse_chan *ch);


struct mount_opts *parse_mount_opts(struct fuse_args *args);
void destroy_mount_opts(struct mount_opts *mo);
void fuse_mount_help(void);
void fuse_mount_version(void);

void fuse_kern_unmount(const char *mountpoint, int fd);
int fuse_kern_mount(const char *mountpoint, struct mount_opts *mo);

int fuse_send_reply_iov_nofree(fuse_req_t req, int error, struct iovec *iov,
			       int count);
void fuse_free_req(fuse_req_t req);

void cuse_lowlevel_init(fuse_req_t req, fuse_ino_t nodeide, const void *inarg);

int fuse_start_thread(pthread_t *thread_id, void *(*func)(void *), void *arg);

int fuse_session_receive_buf_int(struct fuse_session *se, struct fuse_buf *buf,
				 struct fuse_chan *ch);
void fuse_session_process_buf_int(struct fuse_session *se,
				  const struct fuse_buf *buf, struct fuse_chan *ch);
