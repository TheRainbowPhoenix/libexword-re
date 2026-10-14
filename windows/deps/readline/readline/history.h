#ifndef EXWORD_HISTORY_H
#define EXWORD_HISTORY_H
#ifdef __cplusplus
extern "C" {
#endif
void add_history(const char *line);
int read_history(const char *filename);
int write_history(const char *filename);
#ifdef __cplusplus
}
#endif
#endif
