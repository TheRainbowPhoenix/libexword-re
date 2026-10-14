/* Windows implementation of the small iconv subset used by libexword.
 * Based on the cross-build shim supplied in libexword-re-windows.zip.
 * UTF-16BE is the wire format; Windows API uses native little endian wchar_t.
 */
#include "iconv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#define CP_UTF16BE 0x1201BEu
#define CP_LOCALE  0x0ACE0CAu
struct iconv_state { uint32_t from_cp, to_cp; };
static uint32_t parse_encoding(const char *s) {
    char *end;
    unsigned long n;
    if (!s) return 0;
    if (!*s) return CP_LOCALE;
    if (!_stricmp(s,"UTF-8") || !_stricmp(s,"UTF8")) return CP_UTF8;
    if (!_stricmp(s,"UTF-16BE") || !_stricmp(s,"UTF16BE") || !_stricmp(s,"UTF-16")) return CP_UTF16BE;
    if (!_stricmp(s,"SHIFT_JIS") || !_stricmp(s,"SHIFT-JIS") || !_stricmp(s,"SJIS") ||
        !_stricmp(s,"MS-KANJI") || !_stricmp(s,"CP932")) return 932;
    if ((s[0]=='c'||s[0]=='C') && (s[1]=='p'||s[1]=='P')) s += 2;
    n = strtoul(s, &end, 10);
    if (end != s && !*end && n>0 && n<=65535) return (uint32_t)n;
    return 0;
}
iconv_t iconv_open(const char *to, const char *from) {
    struct iconv_state *st;
    uint32_t f = parse_encoding(from), t = parse_encoding(to);
    if (!f || !t) { errno = EINVAL; return (iconv_t)-1; }
    st = (struct iconv_state*)malloc(sizeof(*st));
    if (!st) { errno=ENOMEM; return (iconv_t)-1; }
    st->from_cp=f; st->to_cp=t;
    return (iconv_t)st;
}
int iconv_close(iconv_t cd) {
    if (!cd || cd==(iconv_t)-1) { errno=EBADF; return -1; }
    free(cd); return 0;
}
size_t iconv(iconv_t cd, char **inbuf, size_t *inleft, char **outbuf, size_t *outleft) {
    const unsigned char *in;
    unsigned char *out;
    wchar_t *wide;
    uint32_t f,t;
    size_t n;
    int count, wrote, i;
    if (!cd || cd==(iconv_t)-1) { errno=EBADF; return (size_t)-1; }
    if (!inbuf || !*inbuf) return 0;
    if (!inleft || !outbuf || !*outbuf || !outleft) { errno=EINVAL; return (size_t)-1; }
    n=*inleft; if (!n) return 0;
    f=((struct iconv_state*)cd)->from_cp;
    t=((struct iconv_state*)cd)->to_cp;
    in=(const unsigned char*)*inbuf;
    out=(unsigned char*)*outbuf;
    /* Conversion is atomic; on E2BIG caller must retry with a larger buffer. */
    wide=(wchar_t*)malloc((n+1)*sizeof(wchar_t));
    if (!wide) { errno=ENOMEM; return (size_t)-1; }
    if (f==CP_UTF16BE) {
        if (n & 1) { free(wide); errno=EINVAL; return (size_t)-1; }
        count=(int)(n/2);
        for (i=0;i<count;i++) wide[i]=(wchar_t)(((unsigned int)in[2*i]<<8)|in[2*i+1]);
    } else {
        count=MultiByteToWideChar(f==CP_LOCALE?GetACP():f, 0, (const char*)in, (int)n, wide, (int)(n+1));
        if (!count) { DWORD e=GetLastError(); free(wide); errno=e==ERROR_NO_UNICODE_TRANSLATION?EILSEQ:EINVAL; return (size_t)-1; }
    }
    if (t==CP_UTF16BE) {
        size_t need=(size_t)count*2;
        if (need>*outleft) { free(wide); errno=E2BIG; return (size_t)-1; }
        for (i=0;i<count;i++) { out[2*i]=(unsigned char)(wide[i]>>8); out[2*i+1]=(unsigned char)wide[i]; }
        wrote=(int)need;
    } else {
        wrote=WideCharToMultiByte(t==CP_LOCALE?GetACP():t, 0, wide, count,
                                  (char*)out, (int)*outleft, NULL,NULL);
        if (!wrote) { DWORD e=GetLastError(); free(wide); errno=e==ERROR_INSUFFICIENT_BUFFER?E2BIG:EILSEQ; return (size_t)-1; }
    }
    free(wide);
    *inbuf += n;
    *inleft=0;
    *outbuf += wrote;
    *outleft -= (size_t)wrote;
    return 0;
}
