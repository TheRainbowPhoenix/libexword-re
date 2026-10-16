#ifndef EXWORD_BACKUP_H
#define EXWORD_BACKUP_H
#include "exword.h"

/* Dump the current remote directory into a NEW local directory. No device writes. */
int exword_dump_tree(exword_t *device, const char *remote, const char *destination);
#endif
