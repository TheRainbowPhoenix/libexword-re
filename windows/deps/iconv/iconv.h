/* Minimal GNU iconv-compatible API for the Windows cross-build.
 * Copyright (c) contributors; distributed with libexword-re under GPL-2.0.
 */
#ifndef EXWORD_ICONV_H
#define EXWORD_ICONV_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *iconv_t;
iconv_t iconv_open(const char *to, const char *from);
size_t iconv(iconv_t cd, char **inbuf, size_t *inleft,
             char **outbuf, size_t *outleft);
int iconv_close(iconv_t cd);
#ifdef __cplusplus
}
#endif
#endif
