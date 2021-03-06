// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2018 Drew Richardson <drewrichardson@gmail.com>

#if !defined(DR_H)
#define DR_H

#include "dr_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dr_version.h"
// DR Should be included in dr_types.h
#include "dr_types_common.h"
#include "dr_types.h"
#include "dr_compiler.h"

extern char *restrict dr_optarg;
extern int dr_optind, dr_opterr, dr_optopt, dr_optreset;

int dr_getopt(int argc, char * const argv[], const char *optstring);
int dr_getopt_long(int argc, char **argv, const char *optstring, const struct dr_option *longopts, int *idx);

#define DR_ERR_ISO_C 1
#if defined(_WIN32)
#define DR_ERR_WIN   2
#endif
#define DR_ERR_GAI   3

#define DR_RESULT_OK(TNAME, VALUE) \
  (struct dr_result_##TNAME) { \
    .private_is_err = false, \
    .private_u.private_value = VALUE, \
  }

#define DR_RESULT_ERRNUM(TNAME, DOMAIN, NUM) \
  (struct dr_result_##TNAME) { \
    .private_is_err = true, \
    .private_u.private_error.func = __func__, \
    .private_u.private_error.file = __FILE__, \
    .private_u.private_error.line = __LINE__, \
    .private_u.private_error.domain = DOMAIN, \
    .private_u.private_error.num = NUM, \
  }

#define DR_RESULT_ERRNO(TNAME) DR_RESULT_ERRNUM(TNAME, DR_ERR_ISO_C, errno)
#define DR_RESULT_GETLASTERROR(TNAME) DR_RESULT_ERRNUM(TNAME, DR_ERR_WIN, GetLastError())
#define DR_RESULT_WSAGETLASTERROR(TNAME) DR_RESULT_ERRNUM(TNAME, DR_ERR_WIN, WSAGetLastError())

#define DR_RESULT_ERROR(TNAME, ERROR) \
  (struct dr_result_##TNAME) { \
    .private_is_err = true, \
    .private_u.private_error = *(ERROR), \
  }

#define DR_IF_RESULT_ERR(R, ERROR) \
  if (dr_unlikely((R).private_is_err)) { \
    const struct dr_error *restrict const ERROR = &(R).private_u.private_error; \

#define DR_ELIF_RESULT_OK(TYPE, R, VALUE) \
  } else { \
    TYPE const VALUE = (R).private_u.private_value; \

#define DR_IF_RESULT_OK(TYPE, R, VALUE) \
  if (dr_likely(!(R).private_is_err)) { \
    TYPE const VALUE = (R).private_u.private_value; \

#define DR_ELIF_RESULT_ERR(R, ERROR) \
  } else { \
    const struct dr_error *restrict const ERROR = &(R).private_u.private_error; \

#define DR_FI_RESULT }

#define DR_RESULT_OK_VOID() \
  (struct dr_result_void) { \
    .private_is_err = false, \
  }

#define DR_RESULT_ERRNUM_VOID(DOMAIN, NUM) \
  (struct dr_result_void) { \
    .private_is_err = true, \
    .private_u.private_error.func = __func__, \
    .private_u.private_error.file = __FILE__, \
    .private_u.private_error.line = __LINE__, \
    .private_u.private_error.domain = DOMAIN, \
    .private_u.private_error.num = NUM, \
  }

#define DR_RESULT_ERRNO_VOID() DR_RESULT_ERRNUM_VOID(DR_ERR_ISO_C, errno)
#define DR_RESULT_GETLASTERROR_VOID() DR_RESULT_ERRNUM_VOID(DR_ERR_WIN, GetLastError())
#define DR_RESULT_WSAGETLASTERROR_VOID() DR_RESULT_ERRNUM_VOID(DR_ERR_WIN, WSAGetLastError())

#define DR_RESULT_ERROR_VOID(ERROR) \
  (struct dr_result_void) { \
    .private_is_err = true, \
    .private_u.private_error = *(ERROR), \
  }

#define DR_ELIF_RESULT_OK_VOID(R) \
  } else {

#define DR_IF_RESULT_OK_VOID(R) \
  if (dr_likely(!(R).private_is_err)) {

#define dr_log(msg) dr_log_impl(__func__, __FILE__, __LINE__, msg)
//__attribute__((noinline,cold))
void dr_log_impl(const char *restrict const func, const char *restrict const file, const int line, const char *restrict const msg);

#define dr_log_error(msg, error) dr_log_error_impl(__func__, __FILE__, __LINE__, msg, error)
//__attribute__((noinline,cold))
void dr_log_error_impl(const char *restrict const func, const char *restrict const file, const int line, const char *restrict const msg, const struct dr_error *restrict const error);

#define dr_assert(cond) (dr_likely(cond) ? (void)(0) : dr_assert_fail(__func__, __FILE__, __LINE__, #cond))
//__attribute__((noinline,cold))
NORETURN void dr_assert_fail(const char *restrict const func, const char *restrict const file, const int line, const char *restrict const cond);

int dr_log_format(char *restrict const buf, size_t size, const struct dr_error *restrict const error);

WARN_UNUSED_RESULT bool dr_str_eq(const struct dr_str *restrict const a, const struct dr_str *restrict const b);

#define DR_NS_PER_S ((int64_t)1000000000)
#define DR_NS_PER_MS ((int64_t)1000000)

// 2^63/1000000000 = 9223372036 -> Sep 21 00:12:44 UTC 1677 - Apr 11 23:47:16 UTC 2262
WARN_UNUSED_RESULT struct dr_result_int64 dr_system_time_ns(void);
WARN_UNUSED_RESULT struct dr_result_void dr_system_sleep_ns(const int64_t time);

WARN_UNUSED_RESULT struct dr_result_size dr_read(dr_handle_t fd, void *restrict const buf, size_t count);
WARN_UNUSED_RESULT struct dr_result_size dr_write(dr_handle_t fd, const void *restrict const buf, size_t count);
void dr_close(dr_handle_t fd);

#if defined(_WIN32)

struct _OVERLAPPED;

WARN_UNUSED_RESULT struct dr_result_size dr_read_ol(dr_handle_t fd, void *restrict const buf, size_t count, struct _OVERLAPPED *restrict const ol);
WARN_UNUSED_RESULT struct dr_result_size dr_write_ol(dr_handle_t fd, const void *restrict const buf, size_t count, struct _OVERLAPPED *restrict const ol);

#endif

#define DR_QUEUE_READABLE(c) ((c)->write_pos >= (c)->read_pos ? (c)->write_pos - (c)->read_pos : sizeof((c)->buf) - (c)->read_pos)
#define DR_QUEUE_WRITABLE(c) ((c)->write_pos < (c)->read_pos ? (c)->read_pos - 1 - (c)->write_pos : (c)->read_pos == 0 ? sizeof((c)->buf) - 1 - (c)->write_pos : sizeof((c)->buf) - (c)->write_pos)

WARN_UNUSED_RESULT struct dr_result_void dr_console_startup(void);
extern dr_handle_t dr_stdin;
extern dr_handle_t dr_stdout;
extern dr_handle_t dr_stderr;

#define DR_NONBLOCK  (1U<<0) // DR Should this just always be the default? Doesn't play well on windows
#define DR_CLOEXEC   (1U<<1)
#define DR_REUSEADDR (1U<<2)

struct sockaddr; // DR ...

WARN_UNUSED_RESULT struct dr_result_void dr_socket_startup(void);
WARN_UNUSED_RESULT struct dr_result_handle dr_socket(int domain, int type, int protocol, unsigned int flags);
WARN_UNUSED_RESULT struct dr_result_handle dr_accept(dr_handle_t sockfd, struct sockaddr *restrict const addr, dr_socklen_t *restrict const addrlen, unsigned int flags);
WARN_UNUSED_RESULT struct dr_result_void dr_bind(dr_handle_t sockfd, const struct sockaddr *restrict const addr, dr_socklen_t addrlen);
WARN_UNUSED_RESULT struct dr_result_handle dr_sock_bind(const char *restrict const hostname, const char *restrict const port, unsigned int flags);
WARN_UNUSED_RESULT struct dr_result_void dr_connect(dr_handle_t sockfd, const struct sockaddr *restrict const addr, dr_socklen_t addrlen);
WARN_UNUSED_RESULT struct dr_result_handle dr_sock_connect(const char *restrict const hostname, const char *restrict const port, unsigned int flags);
WARN_UNUSED_RESULT struct dr_result_void dr_listen(dr_handle_t sockfd, int backlog);

WARN_UNUSED_RESULT struct dr_result_handle dr_pipe_bind(const char *restrict const name, unsigned int flags);
WARN_UNUSED_RESULT struct dr_result_handle dr_pipe_connect(const char *restrict const name, unsigned int flags);

#define DR_EVENT_IN  (1U<<0)
#define DR_EVENT_OUT (1U<<1)

#define DR_EVENT_ADD 0
#define DR_EVENT_MOD 1
#define DR_EVENT_DEL 2

WARN_UNUSED_RESULT void *dr_event_key(struct dr_event *restrict const events, int i);
WARN_UNUSED_RESULT bool dr_event_is_read(struct dr_event *restrict const events, int i);
WARN_UNUSED_RESULT bool dr_event_is_write(struct dr_event *restrict const events, int i);

WARN_UNUSED_RESULT struct dr_result_void dr_equeue_init(struct dr_equeue *restrict const e);
void dr_equeue_destroy(struct dr_equeue *restrict const e);

WARN_UNUSED_RESULT struct dr_result_uint dr_equeue_dequeue(struct dr_equeue *restrict const e, struct dr_event *restrict const events, size_t bytes);

void dr_equeue_server_init(struct dr_equeue_server *restrict const s, dr_handle_t fd);
void dr_equeue_server_destroy(struct dr_equeue_server *restrict const s);

void dr_equeue_client_init(struct dr_equeue_client *restrict const c, dr_handle_t fd);
void dr_equeue_client_destroy(struct dr_equeue_client *restrict const c);

WARN_UNUSED_RESULT struct dr_result_handle dr_equeue_accept(struct dr_equeue *restrict const e, struct dr_equeue_server *restrict const s);
WARN_UNUSED_RESULT struct dr_result_size dr_equeue_read(struct dr_equeue *restrict const e, struct dr_equeue_client *restrict const c, void *restrict const buf, const size_t count);
WARN_UNUSED_RESULT struct dr_result_size dr_equeue_write(struct dr_equeue *restrict const e, struct dr_equeue_client *restrict const c, const void *restrict const buf, const size_t count);
WARN_UNUSED_RESULT struct dr_result_void dr_equeue_dispatch(struct dr_equeue *restrict const e);

WARN_UNUSED_RESULT struct dr_result_void dr_task_create(struct dr_task *restrict const task, const size_t stack_size, const dr_task_start_t func, void *restrict const arg);
WARN_UNUSED_RESULT struct dr_task *dr_task_self(void);
void dr_task_destroy(struct dr_task *restrict const task);
void dr_task_runnable(struct dr_task *restrict const task);
NORETURN void dr_task_exit(void *restrict const arg, void (*cleanup)(void *restrict const));
void dr_schedule(const bool sleep);

void dr_wait_init(struct dr_wait *restrict const wait);
void dr_wait_destroy(struct dr_wait *restrict const wait);
void dr_wait_notify(struct dr_wait *restrict const wait);
void dr_wait_wait(struct dr_wait *restrict const wait);

WARN_UNUSED_RESULT struct dr_result_void dr_sem_init(struct dr_sem *restrict const sem, unsigned int value);
void dr_sem_destroy(struct dr_sem *restrict const sem);
WARN_UNUSED_RESULT struct dr_result_void dr_sem_post(struct dr_sem *restrict const sem);
WARN_UNUSED_RESULT struct dr_result_void dr_sem_wait(struct dr_sem *restrict const sem);

// mode
#define DR_DIR    0x80000000
#define DR_APPEND 0x40000000
#define DR_EXCL   0x20000000
#define DR_AUTH   0x08000000
#define DR_TMP    0x04000000
#define DR_RUSR   0x00000100
#define DR_WUSR   0x00000080
#define DR_XUSR   0x00000040
#define DR_RGRP   0x00000020
#define DR_WGRP   0x00000010
#define DR_XGRP   0x00000008
#define DR_ROTH   0x00000004
#define DR_WOTH   0x00000002
#define DR_XOTH   0x00000001

// qid type
#define DR_QTDIR    0x80
#define DR_QTAPPEND 0x40
#define DR_QTEXCL   0x20
#define DR_QTAUTH   0x08
#define DR_QTTMP    0x04

#define	DR_AEXEC  0x1
#define	DR_AWRITE 0x2
#define	DR_AREAD  0x4

#define	DR_OREAD  0
#define	DR_OWRITE 1
#define	DR_ORDWR  2
#define	DR_OEXEC  3
#define	DR_OTRUNC 0x10

WARN_UNUSED_RESULT struct dr_result_file dr_vfs_walk(const struct dr_user *restrict const user, const struct dr_file *restrict const file, const struct dr_str *restrict const name);
WARN_UNUSED_RESULT struct dr_result_fd dr_vfs_open(const struct dr_user *restrict const user, struct dr_file *restrict const file, const uint8_t mode);
WARN_UNUSED_RESULT struct dr_result_uint32 dr_vfs_read(const struct dr_fd *restrict const fd, const uint64_t offset, const uint32_t count, void *restrict const buf);
WARN_UNUSED_RESULT struct dr_result_uint32 dr_vfs_write(const struct dr_fd *restrict const fd, const uint64_t offset, const uint32_t count, const void *restrict const buf);
void dr_vfs_close(struct dr_fd *restrict const fd);

extern struct dr_file_vtbl dr_dir_vtbl;

#define DR_TVERSION 100
#define DR_RVERSION 101
#define DR_TAUTH    102

#define DR_TATTACH  104
#define DR_RATTACH  105

#define DR_RERROR   107

#define DR_TWALK    110
#define DR_RWALK    111
#define DR_TOPEN    112
#define DR_ROPEN    113
#define DR_TCREATE  114
#define DR_RCREATE  115
#define DR_TREAD    116
#define DR_RREAD    117
#define DR_TWRITE   118
#define DR_RWRITE   119
#define DR_TCLUNK   120
#define DR_RCLUNK   121
#define DR_TREMOVE  122
#define DR_RREMOVE  123
#define DR_TSTAT    124
#define DR_RSTAT    125
#define DR_TWSTAT   126
#define DR_RWSTAT   127

#define DR_NOFID ((uint32_t)~0)

WARN_UNUSED_RESULT uint8_t dr_decode_uint8(const uint8_t *restrict const buf);
WARN_UNUSED_RESULT uint16_t dr_decode_uint16(const uint8_t *restrict const buf);
WARN_UNUSED_RESULT uint32_t dr_decode_uint32(const uint8_t *restrict const buf);
WARN_UNUSED_RESULT uint64_t dr_decode_uint64(const uint8_t *restrict const buf);

void dr_encode_uint8(uint8_t *restrict const buf, const uint8_t val);
void dr_encode_uint16(uint8_t *restrict const buf, const uint16_t val);
void dr_encode_uint32(uint8_t *restrict const buf, const uint32_t val);
void dr_encode_uint64(uint8_t *restrict const buf, const uint64_t val);

#define FAIL_UINT32 ((uint32_t)~0)
WARN_UNUSED_RESULT uint32_t dr_9p_decode_stat(struct dr_9p_stat *restrict const stat, const uint8_t *restrict const buf, const uint32_t size);
WARN_UNUSED_RESULT bool dr_9p_decode_header(uint8_t *restrict const type, uint16_t *restrict const tag, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tversion(uint32_t *restrict const msize, struct dr_str *restrict const version, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rversion(uint32_t *restrict const msize, struct dr_str *restrict const version, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tauth(uint32_t *restrict const afid, struct dr_str *restrict const uname, struct dr_str *restrict const aname, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rerror(struct dr_str *restrict const ename, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tattach(uint32_t *restrict const fid, uint32_t *restrict const afid, struct dr_str *restrict const uname, struct dr_str *restrict const aname, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rattach(struct dr_9p_qid *restrict const qid, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Twalk_iterator(uint32_t *restrict const fid, uint32_t *restrict const newfid, uint16_t *restrict const nwname, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Twalk_advance(struct dr_str *restrict const wname, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Twalk_finish(const uint32_t size, const uint32_t pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rwalk_iterator(uint16_t *restrict const nwqid, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rwalk_advance(struct dr_9p_qid *restrict const qid, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rwalk_finish(const uint32_t size, const uint32_t pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Topen(uint32_t *restrict const fid, uint8_t *restrict const mode, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Ropen(struct dr_9p_qid *restrict const qid, uint32_t *restrict const iounit, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tcreate(uint32_t *restrict const fid, struct dr_str *restrict const name, uint32_t *restrict const perm, uint8_t *restrict const mode, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rcreate(struct dr_9p_qid *restrict const qid, uint32_t *restrict const iounit, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tread(uint32_t *restrict const fid, uint64_t *restrict const offset, uint32_t *restrict const count, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rread(uint32_t *restrict const count, const void *restrict *restrict const data, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Twrite(uint32_t *restrict const fid, uint64_t *restrict const offset, uint32_t *restrict const count, const void *restrict *restrict const data, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rwrite(uint32_t *restrict const count, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tclunk(uint32_t *restrict const fid, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rclunk(const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tremove(uint32_t *restrict const fid, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rremove(const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Tstat(uint32_t *restrict const fid, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rstat(struct dr_9p_stat *restrict const stat, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Twstat(uint32_t *restrict const fid, struct dr_9p_stat *restrict const stat, const uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos);
WARN_UNUSED_RESULT bool dr_9p_decode_Rwstat(const uint32_t size, uint32_t *restrict const pos);

WARN_UNUSED_RESULT bool dr_9p_encode_Tversion(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t msize, const struct dr_str *restrict const version);
WARN_UNUSED_RESULT bool dr_9p_encode_Rversion(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t msize, const struct dr_str *restrict const version);
void dr_9p_encode_Rerror(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const struct dr_str *restrict const ename);
void dr_9p_encode_Rerror_err(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const struct dr_error *restrict const error);
WARN_UNUSED_RESULT bool dr_9p_encode_Tattach(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const uint32_t afid, const struct dr_str *restrict const uname, const struct dr_str *restrict const aname);
WARN_UNUSED_RESULT bool dr_9p_encode_Rattach(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const struct dr_file *restrict const f);
WARN_UNUSED_RESULT bool dr_9p_encode_Twalk_iterator(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const uint32_t newfid, uint16_t *restrict const nwname);
WARN_UNUSED_RESULT bool dr_9p_encode_Twalk_add(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, uint16_t *restrict const nwname, const struct dr_str *restrict const name);
WARN_UNUSED_RESULT bool dr_9p_encode_Twalk_finish(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t nwname);
WARN_UNUSED_RESULT bool dr_9p_encode_Rwalk_iterator(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, uint16_t *restrict const nwqid);
WARN_UNUSED_RESULT bool dr_9p_encode_Rwalk_add(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, uint16_t *restrict const nwqid, const struct dr_file *restrict const f);
WARN_UNUSED_RESULT bool dr_9p_encode_Rwalk_finish(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t nwqid);
WARN_UNUSED_RESULT bool dr_9p_encode_Topen(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const uint8_t mode);
WARN_UNUSED_RESULT bool dr_9p_encode_Ropen(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const struct dr_file *restrict const f, const uint32_t iounit);
WARN_UNUSED_RESULT bool dr_9p_encode_Tcreate(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const struct dr_str *restrict const name, const uint32_t perm, const uint8_t mode);
WARN_UNUSED_RESULT bool dr_9p_encode_Rcreate(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const struct dr_file *restrict const f, const uint32_t iounit);
WARN_UNUSED_RESULT bool dr_9p_encode_Tread(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const uint64_t offset, const uint32_t count);
WARN_UNUSED_RESULT bool dr_9p_encode_Rread_iterator(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag);
WARN_UNUSED_RESULT bool dr_9p_encode_Rread_finish(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint32_t count);
WARN_UNUSED_RESULT bool dr_9p_encode_Twrite_iterator(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const uint64_t offset);
WARN_UNUSED_RESULT bool dr_9p_encode_Twrite_finish(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint32_t count);
WARN_UNUSED_RESULT bool dr_9p_encode_Rwrite(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t count);
WARN_UNUSED_RESULT bool dr_9p_encode_Tclunk(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid);
WARN_UNUSED_RESULT bool dr_9p_encode_Rclunk(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag);
WARN_UNUSED_RESULT bool dr_9p_encode_Tremove(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid);
WARN_UNUSED_RESULT bool dr_9p_encode_Rremove(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag);
WARN_UNUSED_RESULT bool dr_9p_encode_Tstat(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid);
WARN_UNUSED_RESULT bool dr_9p_encode_Rstat(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const struct dr_file *restrict const f);
WARN_UNUSED_RESULT bool dr_9p_encode_Twstat(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag, const uint32_t fid, const struct dr_9p_stat *restrict const stat);
WARN_UNUSED_RESULT bool dr_9p_encode_Rwstat(uint8_t *restrict const buf, const uint32_t size, uint32_t *restrict const pos, const uint16_t tag);

#endif // DR_H
