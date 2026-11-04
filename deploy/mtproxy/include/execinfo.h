/*
 * Minimal <execinfo.h> shim for musl (Alpine), used to build upstream MTProxy.
 *
 * MTProxy's common/server-functions.c includes <execinfo.h> unconditionally to
 * print a stack backtrace when it aborts. glibc provides that header and the
 * backtrace()/backtrace_symbols_fd() pair; musl ships neither (and Alpine does
 * not package libexecinfo), so the build fails with
 *   fatal error: execinfo.h: No such file or directory
 *
 * This shim implements the same two functions on top of libgcc's unwinder,
 * which is available in every gcc toolchain, and names frames through dladdr()
 * like glibc does (falling back to the raw address). Only the two functions
 * MTProxy calls are provided. On glibc this file is never used.
 */
#ifndef MTP_EXECINFO_SHIM_H
#define MTP_EXECINFO_SHIM_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unwind.h>

struct __mt_bt_state {
  void **buffer;
  int capacity;
  int count;
};

static inline _Unwind_Reason_Code __mt_bt_callback(struct _Unwind_Context *ctx,
                                                   void *arg) {
  struct __mt_bt_state *st = (struct __mt_bt_state *)arg;
  uintptr_t ip = _Unwind_GetIP(ctx);
  if (!ip) {
    return _URC_NO_REASON;
  }
  if (st->count >= st->capacity) {
    return _URC_END_OF_STACK;
  }
  st->buffer[st->count++] = (void *)ip;
  return _URC_NO_REASON;
}

static inline int backtrace(void **buffer, int size) {
  struct __mt_bt_state st;
  if (!buffer || size <= 0) {
    return 0;
  }
  st.buffer = buffer;
  st.capacity = size;
  st.count = 0;
  _Unwind_Backtrace(__mt_bt_callback, &st);
  return st.count;
}

static inline void backtrace_symbols_fd(void *const *buffer, int size, int fd) {
  char line[512];
  for (int i = 0; i < size; i++) {
    Dl_info info;
    int len;
    if (buffer[i] && dladdr(buffer[i], &info) && info.dli_sname && info.dli_saddr) {
      len = snprintf(line, sizeof line, "%p %s+0x%lx\n", buffer[i],
                     info.dli_sname,
                     (unsigned long)((const char *)buffer[i] -
                                     (const char *)info.dli_saddr));
    } else if (buffer[i] && dladdr(buffer[i], &info) && info.dli_fname) {
      len = snprintf(line, sizeof line, "%p %s+0x%lx\n", buffer[i],
                     info.dli_fname,
                     (unsigned long)((const char *)buffer[i] -
                                     (const char *)info.dli_fbase));
    } else {
      len = snprintf(line, sizeof line, "%p <unknown>\n", buffer[i]);
    }
    if (len > 0) {
      ssize_t ignored = write(fd, line, (size_t)len);
      (void)ignored;
    }
  }
}

#endif /* MTP_EXECINFO_SHIM_H */
