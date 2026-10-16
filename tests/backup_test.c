/* Mock device: no USB, credentials or device writes. Parent supplied by runner. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../src/backup.c"

static char cwd[1024];
static int fail_get, bad_name, calls;

void *xmalloc(size_t n) { void *p = malloc(n); assert(p); return p; }
char *utf16_to_locale(char **out, int *size, const char *in, int length)
{
	int i; *size = length / 2; *out = xmalloc(*size);
	for (i = 0; i < *size; ++i) (*out)[i] = in[2 * i + 1];
	return *out;
}
char *locale_to_utf16(char **out, int *size, const char *in, int length)
{
	int i; *size = length * 2; *out = calloc(1, *size); assert(*out);
	for (i = 0; i < length; ++i) (*out)[2 * i + 1] = in[i];
	return *out;
}
int exword_setpath(exword_t *device, uint8_t *path, uint8_t create)
{
	(void)device; assert(create == 0); assert(strlen((char *)path) < sizeof(cwd));
	strcpy(cwd, (char *)path); ++calls; return 0x20;
}
static void entry(exword_dirent_t *e, const char *name, int flags)
{
	e->flags = flags; e->size = (uint16_t)(strlen(name) + 4);
	e->name = (uint8_t *)xmalloc(strlen(name) + 1); strcpy((char *)e->name, name);
}
int exword_list(exword_t *device, exword_dirent_t **out, uint16_t *count)
{
	(void)device; ++calls; *out = calloc(6, sizeof(**out)); assert(*out);
	if (!strcmp(cwd, "\\_SD_00\\")) {
		*count = 3; entry(*out, "ZH012", 1); entry(*out + 1, "ZH013", 1);
		entry(*out + 2, "AUTHINFO.INF", 0);
	} else if (strstr(cwd, "_CONTENT")) {
		*count = 3; entry(*out, "same.bin", 0); entry(*out + 1, "empty", 0);
		entry(*out + 2, bad_name ? "..\\escape" : "NUL", 0);
	} else {
		*count = 2; entry(*out, "_CONTENT", 1); entry(*out + 1, "JPGVWR.INF", 0);
	}
	return 0x20;
}
void exword_free_list(exword_dirent_t *e)
{
	unsigned i; for (i = 0; e[i].name; ++i) free(e[i].name); free(e);
}
int exword_get_file(exword_t *device, char *name, char **data, int *length)
{
	const char *value = strstr(cwd, "ZH013") ? "second" : "first";
	(void)device; ++calls; *data = NULL; *length = 0;
	if (fail_get && !strcmp(name, "same.bin")) return 0x43;
	if (strcmp(name, "empty")) *length = (int)strlen(value);
	*data = xmalloc(*length + 1); memcpy(*data, value, *length); return 0x20;
}
static void content(const char *parent, const char *suffix, const char *expected)
{
	char path[2048], data[32] = {0}; FILE *f;
	snprintf(path, sizeof(path), "%s/%s", parent, suffix);
	f = fopen(path, "rb"); assert(f);
	assert(fread(data, 1, sizeof(data), f) == strlen(expected)); fclose(f);
	assert(!memcmp(data, expected, strlen(expected)));
}
int main(int argc, char **argv)
{
	char root[2048], failed[2048], malicious[2048]; int before;
	char *encoded;
	assert(argc == 2);
	snprintf(root, sizeof(root), "%s/complete", argv[1]);
	snprintf(failed, sizeof(failed), "%s/failed", argv[1]);
	snprintf(malicious, sizeof(malicious), "%s/malicious", argv[1]);
	assert(exword_dump_tree(NULL, "\\_SD_00\\", root));
	assert(!strcmp(cwd, "\\_SD_00\\"));
	content(root, "ZH012/_CONTENT/same.bin", "first");
	content(root, "ZH013/_CONTENT/same.bin", "second");
	content(root, "ZH012/_CONTENT/empty", "");
	content(root, "ZH012/_CONTENT/%4EUL", "first");
	content(root, "AUTHINFO.INF", "first");
	before = calls; assert(!exword_dump_tree(NULL, "\\_SD_00\\", root)); assert(calls == before);
	assert(!save_exclusive(join_path(root, "AUTHINFO.INF", '/'), "bad", 3));
	content(root, "AUTHINFO.INF", "first");
	fail_get = 1; assert(!exword_dump_tree(NULL, "\\_SD_00\\", failed));
	fail_get = 0; bad_name = 1; assert(!exword_dump_tree(NULL, "\\_SD_00\\", malicious));
	assert(!local_name("../escape") && !local_name("..") && !local_name("a\\b"));
	encoded = local_name("file."); assert(!strcmp(encoded, "file%2E")); free(encoded);
	encoded = local_name("a:b"); assert(!strcmp(encoded, "a%3Ab")); free(encoded);
	puts("Backup tests passed: recursion, duplicate basenames, empty files, no-create, exclusive writes, errors and unsafe names.");
	return 0;
}
