// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2018 Drew Richardson <drewrichardson@gmail.com>

#include "dr.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DR_9P_BUF_SIZE (1<<13)

static bool debug;

WARN_UNUSED_RESULT static bool dr_9p_call(dr_handle_t fd, const uint8_t *restrict const tbuf, const uint32_t tsize, uint8_t *restrict const rbuf, const uint32_t rmax_size, uint32_t *restrict const rsize, uint32_t *restrict const rpos, const uint8_t expected_type) {
  size_t bytes;
  {
    const struct dr_result_size r = dr_write(fd, tbuf, tsize);
    DR_IF_RESULT_ERR(r, err) {
      dr_log_error("dr_write failed", err);
      return false;
    } DR_ELIF_RESULT_OK(size_t, r, value) {
      bytes = value;
    } DR_FI_RESULT;
  }
  if (dr_unlikely(bytes != tsize)) {
    dr_log("Short write");
    return false;
  }
  {
    const struct dr_result_size r = dr_read(fd, rbuf, rmax_size);
    DR_IF_RESULT_ERR(r, err) {
      dr_log_error("dr_read failed", err);
      return false;
    } DR_ELIF_RESULT_OK(size_t, r, value) {
      bytes = value;
    } DR_FI_RESULT;
  }
  if (dr_unlikely(bytes == 0)) {
    dr_log("closing client");
    return false;
  }
  if (dr_unlikely(bytes < sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t))) {
    dr_log("Short read");
    return false;
  }
  *rsize = dr_decode_uint32(rbuf);
  if (dr_unlikely(bytes != *rsize)) {
    dr_log("Short read");
    return false;
  }
  uint8_t type;
  uint16_t tag;
  if (dr_unlikely(!dr_9p_decode_header(&type, &tag, rbuf, *rsize, rpos))) {
    dr_log("dr_9p_decode_header failed");
    return false;
  }
  if (dr_unlikely(tag != 0)) {
    dr_log("Mismatched tag");
    return false;
  }
  if (dr_unlikely(type != expected_type)) {
    if (dr_unlikely(type != DR_RERROR)) {
      dr_log("Unexpected type");
    } else {
      struct dr_str ename;
      if (dr_unlikely(!dr_9p_decode_Rerror(&ename, rbuf, *rsize, rpos))) {
	dr_log("dr_9p_decode_Rerror failed");
      } else {
	printf("Rerror '%.*s'\n", ename.len, ename.buf);
      }
    }
    return false;
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_version(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, uint32_t *restrict const msize) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  char version_9p2000[] = {'9','P','2','0','0','0'};
  {
    const struct dr_str version = {
      .len = sizeof(version_9p2000),
      .buf = version_9p2000,
    };
    if (dr_unlikely(!dr_9p_encode_Tversion(tbuf, sizeof(tbuf), &tpos, 0, sizeof(tbuf), &version))) {
      dr_log("dr_9p_encode_Tversion failed");
      return false;
    }
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RVERSION))) {
    return false;
  }
  {
    struct dr_str version;
    if (dr_unlikely(!dr_9p_decode_Rversion(msize, &version, rbuf, rsize, &rpos))) {
      dr_log("dr_9p_decode_Rversion failed");
      return false;
    }
    if (debug) {
      printf("Rversion %" PRIu32 " '%.*s'\n", *msize, version.len, version.buf);
    }
    if (*msize > sizeof(tbuf)) {
      dr_log("Invalid msize");
      return false;
    }
    if (dr_unlikely(version.len != sizeof(version_9p2000) || memcmp(version_9p2000, version.buf, sizeof(version_9p2000)))) {
      dr_log("Unexpected version");
      return false;
    }
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_attach(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const struct dr_str *restrict const uname, const struct dr_str *restrict const aname, struct dr_9p_qid *restrict const qid) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Tattach(tbuf, rmax_size, &tpos, 0, fid, DR_NOFID, uname, aname))) {
    dr_log("dr_9p_encode_Tattach failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RATTACH))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rattach(qid, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Rattach failed");
    return false;
  }
  if (debug) {
    printf("Rattach %" PRIu8 " %" PRIu32 " %" PRIu64 "\n", qid->type, qid->vers, qid->path);
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_walk(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const uint32_t newfid, char *restrict const name) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  uint16_t nwname;
  if (dr_unlikely(!dr_9p_encode_Twalk_iterator(tbuf, rmax_size, &tpos, 0, fid, newfid, &nwname))) {
    dr_log("dr_9p_encode_Twalk_iterator failed");
    return false;
  }
  {
    char *restrict pos = name;
    char *restrict end = pos;
    // DR Can the terminal condition be simplified
    for (; pos != NULL && *pos != '\0' && end != NULL && *end != '\0'; pos = end + 1) {
      bool last = false;
      end = strchr(pos, '/');
      if (end == NULL) {
	end = pos + strlen(pos);
	last = true;
      }
      if (pos == end) {
	continue;
      }
      struct dr_str n = {
	.len = end - pos,
	.buf = pos,
      };
      if (dr_unlikely(!dr_9p_encode_Twalk_add(tbuf, rmax_size, &tpos, &nwname, &n))) {
	dr_log("dr_9p_encode_Twalk_add failed");
	return false;
      }
      if (last) {
	break;
      }
    }
  }
  if (dr_unlikely(!dr_9p_encode_Twalk_finish(tbuf, rmax_size, &tpos, nwname))) {
    dr_log("dr_9p_encode_Twalk_finish failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RWALK))) {
    return false;
  }
  uint16_t nwqid;
  if (dr_unlikely(!dr_9p_decode_Rwalk_iterator(&nwqid, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Rwalk_iterator failed");
    return false;
  }
  if (debug) {
    printf("Rwalk %" PRIu16, nwqid);
  }
  for (size_t i = 0; i < nwqid; ++i) {
    struct dr_9p_qid qid;
    if (dr_unlikely(!dr_9p_decode_Rwalk_advance(&qid, rbuf, rsize, &rpos))) {
      dr_log("dr_9p_decode_Rwalk_advance failed");
      return false;
    }
    if (debug) {
      printf(" %" PRIu8 " %" PRIu32 " %" PRIu64, qid.type, qid.vers, qid.path);
    }
  }
  if (dr_unlikely(!dr_9p_decode_Rwalk_finish(rsize, rpos))) {
    dr_log("dr_9p_decode_Rwalk_finish failed");
    return false;
  }
  if (debug) {
    printf("\n");
  }
  if (dr_unlikely(nwqid != nwname)) {
    dr_log("file not found");
    return false;
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_open(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const uint8_t mode, struct dr_9p_qid *restrict const qid, uint32_t *restrict const iounit) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Topen(tbuf, rmax_size, &tpos, 0, fid, mode))) {
    dr_log("dr_9p_encode_Topen failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_ROPEN))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Ropen(qid, iounit, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Ropen failed");
    return false;
  }
  if (debug) {
    printf("Ropen %" PRIu8 " %" PRIu32 " %" PRIu64 " %" PRIu32 "\n", qid->type, qid->vers, qid->path, *iounit);
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_create(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const struct dr_str *restrict const name, const uint32_t perm, const uint8_t mode, struct dr_9p_qid *restrict const qid, uint32_t *restrict const iounit) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Tcreate(tbuf, rmax_size, &tpos, 0, fid, name, perm, mode))) {
    dr_log("dr_9p_encode_Tcreate failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RCREATE))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rcreate(qid, iounit, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Rcreate failed");
    return false;
  }
  if (debug) {
    printf("Rcreate %" PRIu8 " %" PRIu32 " %" PRIu64 " %" PRIu32 "\n", qid->type, qid->vers, qid->path, *iounit);
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_read(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const uint64_t offset, const uint32_t count, uint32_t *restrict const bytes, const void *restrict *restrict const data) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Tread(tbuf, rmax_size, &tpos, 0, fid, offset, count))) {
    dr_log("dr_9p_encode_Tread failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RREAD))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rread(bytes, data, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Rread failed");
    return false;
  }
  if (debug) {
    printf("Rread %" PRIu32 "\n", *bytes);
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_write(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const uint64_t offset, const uint32_t count, uint32_t *restrict const bytes, const void *restrict const data) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Twrite_iterator(tbuf, rmax_size, &tpos, 0, fid, offset))) {
    dr_log("dr_9p_encode_Twrite_iterator failed");
    return false;
  }
  const uint32_t real_count = count <= rmax_size - tpos ? count : rmax_size - tpos;
  memcpy(tbuf + tpos, data, real_count);
  if (dr_unlikely(!dr_9p_encode_Twrite_finish(tbuf, rmax_size, &tpos, real_count))) {
    dr_log("dr_9p_encode_Twrite_finish failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RWRITE))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rwrite(bytes, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Rwrite failed");
    return false;
  }
  if (debug) {
    printf("Rwrite %" PRIu32 "\n", *bytes);
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_clunk(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Tclunk(tbuf, rmax_size, &tpos, 0, fid))) {
    dr_log("dr_9p_encode_Tclunk failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RCLUNK))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rclunk(rsize, &rpos))) {
    dr_log("dr_9p_decode_Rclunk failed");
    return false;
  }
  if (debug) {
    printf("Rclunk\n");
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_remove(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Tremove(tbuf, rmax_size, &tpos, 0, fid))) {
    dr_log("dr_9p_encode_Tremove failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RREMOVE))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rremove(rsize, &rpos))) {
    dr_log("dr_9p_decode_Rremove failed");
    return false;
  }
  if (debug) {
    printf("Rremove\n");
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_stat(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, struct dr_9p_stat *restrict const stat) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Tstat(tbuf, rmax_size, &tpos, 0, fid))) {
    dr_log("dr_9p_encode_Tstat failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RSTAT))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rstat(stat, rbuf, rsize, &rpos))) {
    dr_log("dr_9p_decode_Rstat failed");
    return false;
  }
  if (debug) {
    printf("Rstat\n");
  }
  return true;
}

WARN_UNUSED_RESULT static bool dr_9p_wstat(dr_handle_t fd, uint8_t *restrict const rbuf, const uint32_t rmax_size, const uint32_t fid, const struct dr_9p_stat *restrict const stat) {
  uint8_t tbuf[DR_9P_BUF_SIZE];
  uint32_t tpos;
  uint32_t rsize;
  uint32_t rpos;
  if (dr_unlikely(!dr_9p_encode_Twstat(tbuf, rmax_size, &tpos, 0, fid, stat))) {
    dr_log("dr_9p_encode_Twstat failed");
    return false;
  }
  if (dr_unlikely(!dr_9p_call(fd, tbuf, tpos, rbuf, rmax_size, &rsize, &rpos, DR_RWSTAT))) {
    return false;
  }
  if (dr_unlikely(!dr_9p_decode_Rwstat(rsize, &rpos))) {
    dr_log("dr_9p_decode_Rwstat failed");
    return false;
  }
  if (debug) {
    printf("Rwstat\n");
  }
  return true;
}

struct client_app {
  bool (*const func)(dr_handle_t, const uint32_t, int, char *restrict *restrict);
  const char *restrict const name;
  const char *restrict const help;
};

static void print_app_usage(const struct client_app *restrict const a) {
  printf("%s %s\n", a->name, a->help);
}

static void format_time(char *restrict const buf, const size_t buf_len, const uint32_t time) {
  time_t t = time;
  strftime(buf, buf_len, "%c", gmtime(&t));
}

WARN_UNUSED_RESULT static bool print_files(const uint32_t count, const void *restrict data) {
  uint32_t pos = 0;
  while (pos < count) {
    struct dr_9p_stat stat;
    const uint32_t read = dr_9p_decode_stat(&stat, (const uint8_t *)data + pos, count - pos);
    if (dr_unlikely(read == FAIL_UINT32)) {
      dr_log("dr_9p_decode_stat failed");
      return false;
    }
    pos += read;
    char buf[64];
    format_time(buf, sizeof(buf), stat.atime);
    printf("%11" PRIo32 " %.*s %.*s %s %20" PRIu64 " %.*s\n", stat.mode, stat.uid.len, stat.uid.buf, stat.gid.len, stat.gid.buf, buf, stat.length, stat.name.len, stat.name.buf);
  }
  return true;
}

WARN_UNUSED_RESULT static bool parse(const char *restrict input, int *restrict const pargc, char *restrict *restrict *restrict argv) {
  char *restrict const output = (char *)malloc(strlen(input) + 1);
  if (output == NULL) {
    return false;
  }
  char *restrict pos = output;
  int argc = 0;

 es_whitespace:
  switch (*input) {
  case '\0':
    goto done;
  case '\t':
  case ' ':
    ++input;
    goto es_whitespace;
  case '\\':
    ++input;
    goto es_whitespace_backslash;
  default:
    ++argc;
    goto es_default;
  }

 es_whitespace_backslash:
  switch (*input) {
  case '\0':
    goto fail;
  default:
    ++argc;
    *pos++ = *input++;
    goto es_default;
  }

 es_default:
  switch (*input) {
  case '\0':
    goto done;
  case '\t':
  case ' ':
    *pos++ = '\0';
    ++input;
    goto es_whitespace;
  case '"':
    ++input;
    goto es_quote;
  case '\'':
    ++input;
    goto es_apostrophe;
  case '\\':
    ++input;
    goto es_backslash;
  default:
    *pos++ = *input++;
    goto es_default;
  }

 es_backslash:
  switch (*input) {
  case '\0':
    goto fail;
  default:
    *pos++ = *input++;
    goto es_default;
  }

 es_quote:
  switch (*input) {
  case '\0':
    goto fail;
  case '"':
    ++input;
    goto es_default;
  case '\\':
    ++input;
    goto es_quote_backslash;
  default:
    *pos++ = *input++;
    goto es_quote;
  }

 es_quote_backslash:
  switch (*input) {
  case '\0':
    goto fail;
  case '"':
  case '\\':
    *pos++ = *input++;
    goto es_quote;
  default:
    *pos++ = '\\';
    *pos++ = *input++;
    goto es_quote;
  }

 es_apostrophe:
  switch (*input) {
  case '\0':
    goto fail;
  case '\'':
    ++input;
    goto es_default;
  default:
    *pos++ = *input++;
    goto es_apostrophe;
  }

 done:
  *pos++ = '\0';

  *pargc = argc;
  *argv = (char **)malloc((argc + 1)*sizeof(**argv));
  pos = output;
  for (int i = 0; i < argc; ++i) {
    (*argv)[i] = pos;
    pos += strlen(pos) + 1;
  }
  if (argc == 0) {
    free(output);
  }
  (*argv)[argc] = NULL;

  return true;

 fail:
  printf("Syntax error: Unexpected EOF\n");
  free(output);
  return false;
}

WARN_UNUSED_RESULT static bool ls(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  uint8_t rbuf[DR_9P_BUF_SIZE];
  if (argc == 1) {
    ++argc;
  }
  for (int i = 1; i < argc; ++i) {
    if (argc > 2) {
      if (i > 1) {
	printf("\n");
      }
      printf("%s:\n", argv[i]);
    }
    if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, argv[i]))) {
      continue;
    }
    {
      struct dr_9p_qid qid;
      uint32_t iounit;
      if (dr_unlikely(!dr_9p_open(fd, rbuf, msize, 1, DR_OREAD, &qid, &iounit))) {
	goto fail_clunk;
      }
      if (dr_unlikely(!(qid.type & DR_QTDIR))) {
	dr_log("Expected a directory");
	goto fail_clunk;
      }
    }
    {
      uint64_t offset = 0;
      while (true) {
	uint32_t count;
	const void *restrict data;
	if (dr_unlikely(!dr_9p_read(fd, rbuf, msize, 1, offset, msize - 24, &count, &data))) {
	  goto fail_clunk;
	}
	if (count == 0) {
	  break;
	}
	if (dr_unlikely(!print_files(count, data))) {
	  goto fail_clunk;
	}
	offset += count;
      }
    }
  fail_clunk:
    if (dr_9p_clunk(fd, rbuf, msize, 1)) {
    }
  }
  return true;
}

WARN_UNUSED_RESULT static bool cat(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  bool result = false;
  uint8_t rbuf[DR_9P_BUF_SIZE];
  if (argc != 2) {
    printf("Usage: cat <file>\n"); // DR ...
    return false;
  }
  if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, argv[1]))) {
    return false;
  }
  {
    struct dr_9p_qid qid;
    uint32_t iounit;
    if (dr_unlikely(!dr_9p_open(fd, rbuf, msize, 1, DR_OREAD, &qid, &iounit))) {
      return false;
    }
    if (dr_unlikely((qid.type & DR_QTDIR))) {
      dr_log("Expected a file");
      goto fail_clunk;
    }
  }
  {
    uint64_t offset = 0;
    while (true) {
      uint32_t count;
      const void *restrict data;
      if (dr_unlikely(!dr_9p_read(fd, rbuf, msize, 1, offset, msize - 24, &count, &data))) {
	goto fail_clunk;
      }
      if (count == 0) {
	break;
      }
      printf("%.*s", count, (const char *)data);
      offset += count;
    }
  }
  result = true;
 fail_clunk:
  return dr_9p_clunk(fd, rbuf, msize, 1) || result;
}

WARN_UNUSED_RESULT static bool write(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  bool result = false;
  uint8_t rbuf[DR_9P_BUF_SIZE];
  if (argc != 3) {
    printf("Usage: write <data> <dest>\n"); // DR ...
    return false;
  }
  if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, argv[2]))) {
    return false;
  }
  {
    struct dr_9p_qid qid;
    uint32_t iounit;
    if (dr_unlikely(!dr_9p_open(fd, rbuf, msize, 1, DR_OWRITE | DR_OTRUNC, &qid, &iounit))) {
      goto fail_clunk;
    }
    if (dr_unlikely((qid.type & DR_QTDIR))) {
      dr_log("Expected a file");
      goto fail_clunk;
    }
  }
  {
    uint64_t offset = 0;
    uint64_t data_len = strlen(argv[1]);
    while (offset < data_len) {
      uint32_t count;
      if (dr_unlikely(!dr_9p_write(fd, rbuf, msize, 1, offset, data_len - offset, &count, argv[1] + offset))) {
	goto fail_clunk;
      }
      if (count == 0) {
	goto fail_clunk;
      }
      offset += count;
    }
  }
  result = true;
 fail_clunk:
  return dr_9p_clunk(fd, rbuf, msize, 1) || result;
}

WARN_UNUSED_RESULT static bool rm(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  uint8_t rbuf[DR_9P_BUF_SIZE];
  if (argc != 2) {
    printf("Usage: rm <file>\n"); // DR ...
    return false;
  }
  if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, argv[1]))) {
    return false;
  }
  return dr_9p_remove(fd, rbuf, msize, 1);
}

WARN_UNUSED_RESULT static bool stat(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  bool result = false;
  uint8_t rbuf[DR_9P_BUF_SIZE];
  if (argc != 2) {
    printf("Usage: stat <file>\n"); // DR ...
    return false;
  }
  if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, argv[1]))) {
    return false;
  }
  struct dr_9p_stat stat;
  if (dr_unlikely(!dr_9p_stat(fd, rbuf, msize, 1, &stat))) {
    goto fail_clunk;
  }
  char buf[64];
  format_time(buf, sizeof(buf), stat.atime);
  printf("%4" PRIx16 " %8" PRIx32 " %2" PRIx8 " %8" PRIx32 " %16" PRIx64 " %11" PRIo32 " %s", stat.type, stat.dev, stat.qid.type, stat.qid.vers, stat.qid.path, stat.mode, buf);
  format_time(buf, sizeof(buf), stat.mtime);
  printf(" %s %20" PRIu64 " %.*s %.*s %.*s %.*s\n", buf, stat.length, stat.name.len, stat.name.buf, stat.uid.len, stat.uid.buf, stat.gid.len, stat.gid.buf, stat.muid.len, stat.muid.buf);
  result = false;
 fail_clunk:
  return dr_9p_clunk(fd, rbuf, msize, 1) || result;
}

WARN_UNUSED_RESULT static bool do_create(dr_handle_t fd, const uint32_t msize, char *restrict const name_buf, const uint32_t mode) {
  bool result = false;
  uint8_t rbuf[DR_9P_BUF_SIZE];
  char *restrict path;
  char *restrict const slash = strrchr(name_buf, '/');
  char *restrict file;
  if (slash == NULL) {
    path = NULL;
    file = name_buf;
  } else {
    path = name_buf;
    *slash = '\0';
    file = slash + 1;
  }
  if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, path))) {
    return false;
  }
  struct dr_str name = {
    .len = strlen(file),
    .buf = file,
  };
  struct dr_9p_qid qid;
  uint32_t iounit;
  if (dr_unlikely(!dr_9p_create(fd, rbuf, msize, 1, &name, mode, DR_OREAD, &qid, &iounit))) {
    goto fail_clunk;
  }
  result = true;
 fail_clunk:
  return dr_9p_clunk(fd, rbuf, msize, 1) || result;
}

WARN_UNUSED_RESULT static bool create(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  if (argc != 3) {
    printf("Usage: create <name> <perm>\n"); // DR ...
    return false;
  }
  return do_create(fd, msize, argv[1], strtol(argv[2], NULL, 0));
}

WARN_UNUSED_RESULT static bool mkdir(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  if (argc != 3) {
    printf("Usage: mkdir <name> <perm>\n"); // DR ...
    return false;
  }
  return do_create(fd, msize, argv[1], DR_DIR | strtol(argv[2], NULL, 0));
}

WARN_UNUSED_RESULT static bool chmod(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  bool result = false;
  uint8_t rbuf[DR_9P_BUF_SIZE];
  if (argc != 3) {
    printf("Usage: chmod <perm> <file>\n"); // DR ...
    return false;
  }
  if (dr_unlikely(!dr_9p_walk(fd, rbuf, msize, 0, 1, argv[2]))) {
    return false;
  }
  struct dr_9p_stat stat = {
    .type = ~((uint16_t)0),
    .dev = ~((uint32_t)0),
    .qid.type = ~((uint8_t)0),
    .qid.vers = ~((uint32_t)0),
    .qid.path = ~((uint64_t)0),
    .mode = strtol(argv[1], NULL, 0),
    .atime = ~((uint32_t)0),
    .mtime = ~((uint32_t)0),
    .length = ~((uint64_t)0),
  };
  if (dr_unlikely(!dr_9p_wstat(fd, rbuf, msize, 1, &stat))) {
    goto fail_clunk;
  }
  result = true;
 fail_clunk:
  return dr_9p_clunk(fd, rbuf, msize, 1) || result;
}

WARN_UNUSED_RESULT static bool sh(dr_handle_t fd, const uint32_t msize, int ignored_argc, char *restrict *restrict ignored_argv);
WARN_UNUSED_RESULT static bool help(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv);

static const struct client_app client_apps[] = {
  { ls, "ls", "<directories>" },
  { cat, "cat", "<file>" },
  { write, "write", "<data> <file>" },
  { rm, "rm", "<name>" },
  { rm, "rmdir", "<name>" },
  { stat, "stat", "<name>" },
  { create, "create", "<name> <perm>" },
  { mkdir, "mkdir", "<name> <perm>" },
  { chmod, "chmod", "<perm> <name>" },
  { sh, "sh", "" },
  { help, "help", ""},
  // DR wstat
};

bool sh(dr_handle_t fd, const uint32_t msize, int ignored_argc, char *restrict *restrict ignored_argv) {
  (void)ignored_argc;
  (void)ignored_argv;
  char buf[1<<8];
  char cwd[1<<8];
  cwd[0] = '/';
  cwd[1] = '\0';
  bool accept_next = true;
  while (accept_next) {
    printf("%s $ ", cwd);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
      printf("\n");
      break;
    }
    const size_t len = strlen(buf);
    if (len == 0) {
      break;
    }
    if (buf[len - 1] != '\n') {
      printf("Line too long\n");
      return false;
    }
    buf[len - 1] = '\0';
    int argc;
    char *restrict *restrict argv;
    if (!parse(buf, &argc, &argv)) {
      continue;
    }
    if (argc == 0) {
    } else if (strcmp("cd", argv[0]) == 0) {
      uint8_t rbuf[DR_9P_BUF_SIZE];
      if (argc < 2) {
	printf("Not enough arguments\n");
      } else if (argc > 2) {
	printf("Too many arguments\n");
      } else if (argv[1][0] == '/') {
	printf("Only relative paths are permited\n");
      } else if (dr_9p_walk(fd, rbuf, msize, 0, 0, argv[1])) {
	char *restrict pos = argv[1];
	char *restrict end = pos;
	// DR Can the terminal condition be simplified
	for (; pos != NULL && *pos != '\0' && end != NULL && *end != '\0'; pos = end + 1) {
	  bool last = false;
	  end = strchr(pos, '/');
	  if (end == NULL) {
	    end = pos + strlen(pos);
	    last = true;
	  }
	  if (pos == end) {
	    continue;
	  }
	  if (end - pos == 2 && pos[0] == '.' && pos[1] == '.') {
	    // Remove section
	    char *restrict const slash = strrchr(cwd, '/');
	    if (slash != NULL) {
	      if (slash == cwd) {
		cwd[1] = '\0';
	      } else {
		*slash = '\0';
	      }
	    }
	  } else if (end - pos == 1 && pos[0] == '.') {
	    // Do nothing
	  } else {
	    if (cwd[1] != '\0') {
	      strcat(cwd, "/");
	    }
	    strncat(cwd, pos, end - pos);
	  }
	  if (last) {
	    break;
	  }
	}
      }
    } else if (strcmp("exit", argv[0]) == 0) {
      accept_next = false;
    } else if (strcmp("sh", argv[0]) == 0) {
      printf("Nice try buster\n");
    } else {
      size_t app;
      for (app = 0; app < sizeof(client_apps)/sizeof(client_apps[0]); ++app) {
	if (strcmp(client_apps[app].name, argv[0]) == 0) {
	  if (dr_unlikely(!client_apps[app].func(fd, msize, argc, argv))) {
	    // Do nothing
	  }
	  break;
	}
      }
      if (app == sizeof(client_apps)/sizeof(client_apps[0])) {
	printf("%s: command not found\n", argv[0]);
      }
    }
    free(argv[0]);
    free((void *)argv);
  }
  return true;
}

bool help(dr_handle_t fd, const uint32_t msize, int argc, char *restrict *restrict argv) {
  (void)fd;
  (void)msize;
  (void)argc;
  (void)argv;
  for (size_t i = 0; i < sizeof(client_apps)/sizeof(client_apps[0]); ++i) {
    print_app_usage(&client_apps[i]);
  }
  return true;
}

WARN_UNUSED_RESULT static int print_version(void) {
  printf("9p_client %u.%u.%u\n", DR_VERSION_MAJOR, DR_VERSION_MINOR, DR_VERSION_PATCH);
  return 0;
}

WARN_UNUSED_RESULT static int print_usage(void) {
  printf("Usage: 9p_client [OPTIONS]... [COMMAND] [COMMAND OPTIONS]...\n"
	 "\n"
	 "Options:\n"
	 "  -a, --address  TCP/IP address name to connect to\n"
	 "  -p, --port     TCP/IP port name to connect to\n"
	 "  -n, --named    Named pipe to connect to\n"
	 "  -u, --uname    User name\n"
	 "  -d, --debug    Print received messages\n"
	 "  -v, --version  Print version information\n"
	 "  -h, --help     Print this help\n"
	 "\n"
	 "Commands:\n"
	 "\n");
  if (help(0, 0, 0, NULL)) {
  }
  return -1;
}

static char none[] = "none";

int main(int argc, char *argv[]) {
  const char *restrict address = 0;
  const char *restrict port = 0;
  const char *restrict named = 0;
  char *restrict uname_buf = none;
  {
    static struct dr_option longopts[] = {
      {"address", 1, 0, 'a'},
      {"port", 1, 0, 'p'},
      {"named", 1, 0, 'n'},
      {"uname", 1, 0, 'u'},
      {"debug", 0, 0, 'd'},
      {"version", 0, 0, 'v'},
      {"help", 0, 0, 'h'},
      {0, 0, 0, 0},
    };
    dr_optind = 0;
    while (true) {
      int opt = dr_getopt_long(argc, argv, "+a:p:n:u:dvh", longopts, NULL);
      if (opt == -1) {
	break;
      }
      switch (opt) {
      case 'a':
	address = dr_optarg;
	break;
      case 'p':
	port = dr_optarg;
	break;
      case 'n':
	named = dr_optarg;
	break;
      case 'u':
	uname_buf = dr_optarg;
	break;
      case 'd':
	debug = true;
	break;
      case 'v':
	return print_version();
      default:
      case 'h':
	return print_usage();
      }
    }
  }
  if (argc <= dr_optind) {
    return print_usage();
  }
  size_t app;
  for (app = 0; app < sizeof(client_apps)/sizeof(client_apps[0]); ++app) {
    if (strcmp(client_apps[app].name, argv[dr_optind]) == 0) {
      break;
    }
  }
  if (app == sizeof(client_apps)/sizeof(client_apps[0])) {
    return print_usage();
  }
  int result = -1;
  dr_handle_t fd;
  if (address != NULL && port != NULL) {
    {
      const struct dr_result_void r = dr_socket_startup();
      DR_IF_RESULT_ERR(r, err) {
	dr_log_error("dr_socket_startup failed", err);
	goto fail;
      } DR_FI_RESULT;
    }
    const struct dr_result_handle r = dr_sock_connect(address, port, DR_CLOEXEC);
    DR_IF_RESULT_ERR(r, err) {
      dr_log_error("dr_sock_connect failed", err);
      goto fail;
    } DR_ELIF_RESULT_OK(dr_handle_t, r, value) {
      fd = value;
    } DR_FI_RESULT;
  } else if (named != NULL) {
    const struct dr_result_handle r = dr_pipe_connect(named, DR_CLOEXEC);
    DR_IF_RESULT_ERR(r, err) {
      dr_log_error("dr_pipe_connect failed", err);
      goto fail;
    } DR_ELIF_RESULT_OK(dr_handle_t, r, value) {
      fd = value;
    } DR_FI_RESULT;
  } else {
    printf("Incomplete connection information provided\n");
    return -1;
  }
  uint32_t msize;
  {
    uint8_t rbuf[DR_9P_BUF_SIZE];
    if (!dr_9p_version(fd, rbuf, sizeof(rbuf), &msize)) {
      goto fail_close_fd;
    }
    {
      const struct dr_str uname = {
	.len = strlen(uname_buf),
	.buf = uname_buf,
      };
      const struct dr_str aname = {
	.len = 0,
      };
      struct dr_9p_qid qid;
      if (!dr_9p_attach(fd, rbuf, msize, 0, &uname, &aname, &qid)) {
	goto fail_close_fd;
      }
      if (dr_unlikely(!(qid.type & DR_QTDIR))) {
	dr_log("Expected a directory");
	goto fail_close_fd;
      }
    }
  }
  if (dr_unlikely(!client_apps[app].func(fd, msize, argc - dr_optind, argv + dr_optind))) {
    goto fail_close_fd;
  }
  {
    uint8_t rbuf[DR_9P_BUF_SIZE];
    if (dr_unlikely(!dr_9p_clunk(fd, rbuf, msize, 0))) {
      goto fail_close_fd;
    }
  }
  result = 0;
 fail_close_fd:
  dr_close(fd);
 fail:
  return result;
}
