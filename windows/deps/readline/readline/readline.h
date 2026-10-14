/* Simple line-oriented readline substitute; no interactive line editing. */
#ifndef EXWORD_READLINE_H
#define EXWORD_READLINE_H
#ifdef __cplusplus
extern "C" {
#endif
char *readline(const char *prompt);
void add_history(const char *line);
#ifdef __cplusplus
}
#endif
#endif
