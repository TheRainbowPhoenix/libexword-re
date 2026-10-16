/* Raw recursive backup: only LIST, GET and SETPATH(no-create) on the device. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "backup.h"
#include "util.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct dump_context {
	exword_t *device;
	FILE *log;
	unsigned files, directories, errors;
	unsigned long long bytes;
};

static char *join_path(const char *base, const char *name, char separator)
{
	size_t n = strlen(base), m = strlen(name);
	char *p = xmalloc(n + m + 2);
	memcpy(p, base, n);
	if (n && base[n - 1] != separator) p[n++] = separator;
	memcpy(p + n, name, m + 1);
	return p;
}

static void record(struct dump_context *ctx, const char *status, const char *remote,
		const char *local, int result, unsigned long long bytes)
{
	fprintf(ctx->log, "%s\t%d\t%llu\t%s\t%s\n", status, result, bytes, remote, local);
	fflush(ctx->log);
	printf("%s %s (%llu bytes, result %d)\n", status, remote, bytes, result);
	fflush(stdout);
	if (ferror(ctx->log)) {
		fprintf(stderr, "Backup manifest write failed.\n");
		ctx->errors++;
	}
}

/* Refuse path traversal; use reversible byte escapes for nonportable local names. */
static char *local_name(const char *name)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t i, n = strlen(name), k = 0, stem;
	int reserved;
	char device_name[9];
	char *out;
	if (!n || !strcmp(name, ".") || !strcmp(name, "..") || n > 240) return NULL;
	for (i = 0; i < n; ++i)
		if ((unsigned char)name[i] < 32 || name[i] == '/' || name[i] == '\\') return NULL;
	stem = strcspn(name, ".");
	memset(device_name, 0, sizeof(device_name));
	if (stem < sizeof(device_name))
		for (i = 0; i < stem; ++i) device_name[i] = (char)toupper((unsigned char)name[i]);
	reserved = !strcmp(device_name, "CON") || !strcmp(device_name, "PRN") ||
		!strcmp(device_name, "AUX") || !strcmp(device_name, "NUL") ||
		(stem == 4 && (!memcmp(device_name, "COM", 3) || !memcmp(device_name, "LPT", 3)) &&
		 device_name[3] >= '0' && device_name[3] <= '9') || !strcmp(name, "_backup-manifest.tsv");
	out = xmalloc(n * 3 + 1);
	for (i = 0; i < n; ++i) {
		unsigned char c = (unsigned char)name[i];
		if (!(reserved && i == 0) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '_' || c == '-' || (c == '.' && i + 1 < n)))
			out[k++] = c;
		else {
			out[k++] = '%'; out[k++] = hex[c >> 4]; out[k++] = hex[c & 15];
		}
	}
	out[k] = 0;
	return out;
}

static char *entry_name(exword_dirent_t *entry)
{
	char *name = NULL, *roundtrip = NULL;
	int length = 0, roundtrip_length = 0;
	size_t bytes;
	if (entry->size < 4 || !entry->name) return NULL;
	bytes = entry->size - 3;
	if (entry->flags & LIST_F_UNICODE) {
		if ((bytes & 1) || bytes < 2 || entry->name[bytes - 1] || entry->name[bytes - 2]) return NULL;
		if (!utf16_to_locale(&name, &length, (char *)entry->name, (int)bytes) || !name) return NULL;
		/* Do not request a substituted name if the PC encoding cannot represent it. */
		if (!locale_to_utf16(&roundtrip, &roundtrip_length, name, (int)strlen(name) + 1) ||
		    roundtrip_length != bytes || memcmp(roundtrip, entry->name, bytes)) {
			free(name); name = NULL;
		}
		free(roundtrip);
	} else {
		if (!memchr(entry->name, 0, bytes) || strlen((char *)entry->name) + 1 != bytes) return NULL;
		name = xmalloc(bytes);
		memcpy(name, entry->name, bytes);
	}
	return name;
}

static int save_exclusive(const char *path, const char *data, int length)
{
	int fd, result = 1;
	FILE *f;
	fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0600);
	if (fd < 0) return 0;
	f = fdopen(fd, "wb");
	if (!f) { close(fd); return 0; }
	if (length && fwrite(data, 1, length, f) != (size_t)length) result = 0;
	if (fclose(f)) result = 0;
	/* On failure retain partial data, but the manifest never marks it OK. */
	return result;
}

static void walk(struct dump_context *ctx, const char *remote, const char *local, unsigned depth)
{
	exword_dirent_t *entries = NULL;
	uint16_t count = 0;
	unsigned i;
	int rsp;
	if (depth > 32) {
		ctx->errors++; record(ctx, "DEPTH_LIMIT", remote, local, -1, 0); return;
	}
	rsp = exword_setpath(ctx->device, (uint8_t *)remote, 0);
	if (rsp == 0x20) rsp = exword_list(ctx->device, &entries, &count);
	if (rsp != 0x20 || (count && !entries)) {
		ctx->errors++; record(ctx, "LIST_FAILED", remote, local, rsp, 0);
		if (entries) exword_free_list(entries);
		return;
	}
	ctx->directories++;
	record(ctx, "DIRECTORY", remote, local, 0x20, 0);
	for (i = 0; i < count; ++i) {
		char *name = entry_name(&entries[i]);
		char *encoded = name ? local_name(name) : NULL;
		char *child_remote, *child_local;
		if (!encoded) {
			ctx->errors++; record(ctx, "UNSUPPORTED_NAME", remote, local, (int)i, 0);
			free(name); continue;
		}
		child_remote = join_path(remote, name, '\\');
		child_local = join_path(local, encoded, '/');
		if (entries[i].flags & LIST_F_DIR) {
			if (mkdir(child_local, 0700)) {
				ctx->errors++; record(ctx, "MKDIR_FAILED", child_remote, child_local, errno, 0);
			} else walk(ctx, child_remote, child_local, depth + 1);
			/* LIST/GET below must run in the parent, never a previous child's folder. */
			rsp = exword_setpath(ctx->device, (uint8_t *)remote, 0);
			if (rsp != 0x20) {
				ctx->errors++; record(ctx, "RETURN_FAILED", remote, local, rsp, 0);
				free(child_remote); free(child_local); free(encoded); free(name); break;
			}
		} else {
			char *data = NULL;
			int length = 0;
			rsp = exword_get_file(ctx->device, name, &data, &length);
			if (rsp != 0x20 || length < 0 || (length && !data)) {
				ctx->errors++; record(ctx, "GET_FAILED", child_remote, child_local, rsp, 0);
			} else if (!save_exclusive(child_local, data, length)) {
				ctx->errors++; record(ctx, "SAVE_FAILED", child_remote, child_local, errno, length);
			} else {
				ctx->files++; ctx->bytes += (unsigned)length;
				record(ctx, "OK", child_remote, child_local, rsp, length);
			}
			free(data);
		}
		free(child_remote); free(child_local); free(encoded); free(name);
	}
	if (entries) exword_free_list(entries);
}

int exword_dump_tree(exword_t *device, const char *remote, const char *destination)
{
	struct dump_context ctx;
	char *manifest;
	int fd, rsp;
	memset(&ctx, 0, sizeof(ctx));
	ctx.device = device;
	/* A new root prevents collisions, accidental overwrites, and stale mixed backups. */
	if (!remote || !*remote || !destination || !*destination || mkdir(destination, 0700)) {
		fprintf(stderr, "Dump needs a NEW local directory with an existing parent. Nothing overwritten.\n");
		return 0;
	}
	manifest = join_path(destination, "_backup-manifest.tsv", '/');
	fd = open(manifest, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0600);
	ctx.log = fd < 0 ? NULL : fdopen(fd, "wb");
	if (!ctx.log) {
		if (fd >= 0) close(fd);
		fprintf(stderr, "Cannot create manifest in %s\n", destination); free(manifest); return 0;
	}
	fprintf(ctx.log, "status\tresult\tbytes\tremote\tlocal\n");
	walk(&ctx, remote, destination, 0);
	rsp = exword_setpath(device, (uint8_t *)remote, 0);
	if (rsp != 0x20) { ctx.errors++; record(&ctx, "RESTORE_FAILED", remote, destination, rsp, 0); }
	fprintf(ctx.log, "SUMMARY\t%u\t%llu\tfiles=%u\tdirectories=%u\n",
		ctx.errors, ctx.bytes, ctx.files, ctx.directories);
	if (fclose(ctx.log)) ctx.errors++;
	printf("Dump %s: %u files, %u directories, %llu bytes, %u errors.\nManifest: %s\n",
		ctx.errors ? "INCOMPLETE" : "complete", ctx.files, ctx.directories, ctx.bytes, ctx.errors, manifest);
	free(manifest);
	return ctx.errors == 0;
}
