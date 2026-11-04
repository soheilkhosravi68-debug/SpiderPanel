/*
 * Minimal glibc -> musl compatibility shim for upstream MTProxy.
 *
 * MTProxy's job scheduler keeps a per-thread `struct drand48_data` and drives it
 * through the _r (reentrant) 48-bit LCG family. Those symbols are glibc
 * extensions: musl provides only the non-reentrant drand48()/lrand48()/mrand48()
 * and has no `struct drand48_data`, so the build fails with
 * "field 'rand_data' has incomplete type".
 *
 * These equivalents implement the same standard 48-bit LCG
 * (a = 0x5DEECE66D, c = 0xB, modulus 2^48) so MTProxy keeps its thread-local,
 * reentrant RNG behaviour on musl without any change to upstream sources.
 * On glibc this header is a no-op.
 */
#ifndef MTP_MUSL_COMPAT_H
#define MTP_MUSL_COMPAT_H

#if defined(__GLIBC__)

/* glibc already provides the full interface. */

#else

#include <stdint.h>

#define MTP_LRAND48_MASK 0xFFFFFFFFFFFFULL
#define MTP_LRAND48_MULT 0x5DEECE66DULL
#define MTP_LRAND48_ADD 0xBULL
#define MTP_LRAND48_DENOM 281474976710656.0 /* 2^48 */

struct drand48_data {
  uint64_t __mt_state;
  unsigned short __init;
};

static inline uint64_t __mt_drand48_step(struct drand48_data *buf) {
  buf->__mt_state =
      (buf->__mt_state * MTP_LRAND48_MULT + MTP_LRAND48_ADD) & MTP_LRAND48_MASK;
  return buf->__mt_state;
}

static inline void __mt_drand48_ensure(struct drand48_data *buf) {
  if (!buf->__init) {
    /* Default srand48() seed value: (0 << 16) | 0x330E */
    buf->__mt_state = 0x330EULL;
    buf->__init = 1;
  }
}

static inline int srand48_r(long seed, struct drand48_data *buf) {
  buf->__mt_state =
      (((uint64_t)(uint32_t)seed) << 16 | 0x330EULL) & MTP_LRAND48_MASK;
  buf->__init = 1;
  return 0;
}

static inline int lrand48_r(struct drand48_data *buf, long *result) {
  __mt_drand48_ensure(buf);
  *result = (long)(__mt_drand48_step(buf) >> 17);
  return 0;
}

static inline int mrand48_r(struct drand48_data *buf, long *result) {
  __mt_drand48_ensure(buf);
  *result = (long)(int32_t)(__mt_drand48_step(buf) >> 16);
  return 0;
}

static inline int drand48_r(struct drand48_data *buf, double *result) {
  __mt_drand48_ensure(buf);
  *result = (double)__mt_drand48_step(buf) / MTP_LRAND48_DENOM;
  return 0;
}

#endif /* __GLIBC__ */
#endif /* MTP_MUSL_COMPAT_H */
