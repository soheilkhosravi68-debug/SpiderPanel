/*
 * Minimal <execinfo.h> shim for musl (Alpine), used to build upstream MTProxy.
 *
 * MTProxy's common/server-functions.c includes <execinfo.h> unconditionally to
 * print a stack backtrace when it aborts. glibc provides that header plus the
 * backtrace()/backtrace_symbols_fd() pair; musl ships neither (and Alpine does
 * not package libexecinfo), so compilation fails with
 *   fatal error: execinfo.h: No such file or directory
 *
 * This shim provides the two functions MTProxy calls so the build is
 * self-contained and cannot fail at link time.
 *
 * Why the backtrace body is intentionally empty: the obvious implementation
 * drives libgcc's unwinder (_Unwind_Backtrace), but that pulls in libgcc_eh,
 * whose frame-unwind registration object references the glibc-only
 * _dl_find_object() symbol on glibc-based toolchains. That fails to link
 * against musl ("undefined reference to `_dl_find_object'"), which would turn a
 * cosmetic diagnostic into a hard build failure. This shim is used only on the
 * abort() path to dump a crash backtrace; returning zero frames keeps the relay
 * fully functional and lets MTProxy's own error message still print.
 *
 * On glibc this file is never used: glibc's real <execinfo.h> wins because this
 * shim is only injected through -I/tmp/mt-shim/include on musl builds.
 */
#ifndef MTP_EXECINFO_SHIM_H
#define MTP_EXECINFO_SHIM_H

#include <stddef.h>

/* MTProxy checks the return value and passes it to backtrace_symbols_fd(). */
static inline int backtrace(void **buffer, int size) {
  (void)buffer;
  (void)size;
  return 0;
}

static inline void backtrace_symbols_fd(void *const *buffer, int size, int fd) {
  (void)buffer;
  (void)size;
  (void)fd;
}

#endif /* MTP_EXECINFO_SHIM_H */
