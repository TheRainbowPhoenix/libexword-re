/* Portable, deliberately minimal subset of GNU readline for Windows.
 * This does not implement arrow-key recall or GNU line editing.
 */
#include "readline/readline.h"
#include "readline/history.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define MAX_HISTORY 64
#define MAX_LINE 8192
static char *history[MAX_HISTORY];
static int history_count, history_head;
char *readline(const char *prompt) {
    char buf[MAX_LINE];
    size_t n;
    char *out;
    if (prompt) { fputs(prompt, stdout); fflush(stdout); }
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    n = strlen(buf);
    if (n && buf[n - 1] == '\n') buf[--n] = 0;
    if (n && buf[n - 1] == '\r') buf[--n] = 0;
    out = (char *)malloc(n + 1);
    if (!out) abort();
    memcpy(out, buf, n + 1);
    return out;
}
void add_history(const char *line) {
    if (!line || !*line) return;
    if (history_count == MAX_HISTORY) free(history[history_head]);
    else history_count++;
    history[history_head] = _strdup(line);
    history_head = (history_head + 1) % MAX_HISTORY;
}
int read_history(const char *filename) {
    char buf[MAX_LINE];
    FILE *f = filename ? fopen(filename, "r") : NULL;
    if (!f) return 0;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        if (n && buf[n-1] == '\n') buf[--n] = 0;
        if (n && buf[n-1] == '\r') buf[--n] = 0;
        add_history(buf);
    }
    fclose(f);
    return 0;
}
int write_history(const char *filename) {
    int i, start;
    FILE *f = filename ? fopen(filename, "w") : NULL;
    if (!f) return 1;
    start = history_count == MAX_HISTORY ? history_head : 0;
    for (i = 0; i < history_count; ++i) {
        int idx = (start + i) % MAX_HISTORY;
        if (history[idx]) fprintf(f, "%s\n", history[idx]);
    }
    fclose(f);
    return 0;
}
