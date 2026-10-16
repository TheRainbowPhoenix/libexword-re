#ifndef EXWORD_USB_TRACE_H
#define EXWORD_USB_TRACE_H

/* Process-wide transport diagnostics, including discovery before a handle exists. */
extern int obex_verbose;
extern int obex_keep_alt;
extern int obex_casio_init;
void obex_trace(const char *format, ...);

#endif
