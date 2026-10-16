/* exword - program for transfering files to Casio-EX-Word dictionaries
 *
 * Copyright (C) 2010 - Brian Johnson <brijohn@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <libgen.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "exword.h"
#include "util.h"
#include "list.h"
#include "usb_trace.h"
#include "backup.h"

struct state {
	exword_t *device;
	int mode;
	int running;
	int connected;
	int debug;
	int mkdir;
	int authenticated;
	int sd_inserted;
	int last_response;
	char *cwd;
	struct list_head cmd_list;
};

typedef void (*cmd_func)(struct state *);

struct cmd_arg {
	char *arg;
	struct list_head queue;
};

struct command {
	char *cmd_str;
	cmd_func ptr;
	char *help_short;
	char *help_long;
};

void quit(struct state *s);
void help(struct state *s);
void connect(struct state *s);
void disconnect(struct state *s);
void set(struct state *s);
void model(struct state *s);
void capacity(struct state *s);
void format(struct state *s);
void list(struct state *s);
void delete(struct state *s);
void send(struct state *s);
void get(struct state *s);
void setpath(struct state *s);
void dict(struct state *s);
void dump(struct state *s);

struct command commands[] = {
{"dump", dump, "dump LOCAL_DIRECTORY\t- recursively back up current device folder\n",
	"Downloads raw files and subfolders into a NEW local directory.\n"
	"No decryption, authentication or device writes. Manifest records failures.\n"
	"Nonportable local names use percent escapes; see the manifest for mapping.\n"},
{"connect", connect, "connect [mode] [region]\t- connect to attached dictionary\n",
	"Connects to device.\n\n"
	"Region specifies the region of the device (default:ja).\n"
	"Mode can be one of the following values:\n"
	"library - connect as CASIO Library (default)\n"
	"text    - connect as Textloader\n"
	"text2   - connect as TextLoader 2 (XD-D6600 diagnostic profile)\n"
	"cd      - connect as CDLoader\n"},
{"disconnect", disconnect, "disconnect\t\t- disconnect from dictionary\n",
	"Disconnects from device.\n"},
{"model", model, "model\t\t\t- display model information\n",
	"Displays model information of device.\n"},
{"capacity", capacity, "capacity\t\t- display medium capacity\n",
	"Displays capacity of current storage medium.\n"},
{"format", format, "format\t\t\t- format SD card\n",
	"Formats currently inserted SD Card.\n"},
{"list", list, "list\t\t\t- list files\n",
	"Lists files and directories under current path.\n\n"
	"Directories are enclosed in <>.\n"
	"Files or directories beginning with * were returned as unicode.\n"},
{"delete", delete, "delete <filename>\t- delete a file\n",
	"Deletes a file from dicionary.\n"},
{"send", send, "send <filename>\t\t- upload a file\n",
	"Uploads a file to dicionary.\n"},
{"get", get, "get <filename>\t\t- download a file\n",
	"Downloads a file from dicionary.\n"},
{"setpath", setpath, "setpath <path>\t\t- changes directory on dictionary\n",
	"Changes to the the specified path.\n\n"
	"<path> is in the form of <sd|mem://<path>\n"
	"Example: mem:/// - sets path to root of internal memory\n"},
{"dict", dict, "dict <sub-function>\t- add-on dictionary commands\n",
	"This command allows manipulation of add-on dictionaries. It uses\n"
	"the storage medium of your current path as the storage device to\n"
	"operate on. The reset sub-function WILL delete all installed\n"
	"dictionaries.\n\n"
	"Sub functions:\n"
	"reset <user>\t  - resets authentication info\n"
	"auth <user> <key> - authenticate to dictionary\n"
	"list\t\t  - list installed add-on dictionaries\n"
	"decrypt <id>\t  - decrypts specified add-on dictionary\n"
	"remove  <id>\t  - removes specified add-on dictionary\n"
	"install <id>\t  - installs specified add-on dictionary\n"},
{"set", set, "set <option> [value]\t- sets program options\n",
	"Sets <option> to [value], if no value is specified will display current value.\n\n"
	"Available options:\n"
	"debug <level>  - sets debug level (0-5)\n"
	"mkdir <on|off> - specifies whether setpath should create directories\n"},
{"exit", quit, "exit\t\t\t- exits program\n",
	"Exits program and disconnects from device.\n"},
{"help", help, NULL, NULL},
{NULL, NULL, NULL, NULL}
};

void load_history()
{
	char * filename;
	const char *data_dir = get_data_dir();
	filename = mkpath(data_dir, ".exword_history");
	read_history(filename);
	free(filename);
}

void store_history()
{
	char * filename;
	const char *data_dir = get_data_dir();
	filename = mkpath(data_dir, ".exword_history");
	write_history(filename);
	free(filename);
}

void queue_arg(struct list_head *head, char * arg)
{
	struct cmd_arg * _arg = xmalloc(sizeof(struct cmd_arg));
	_arg->arg = xmalloc(strlen(arg) + 1);
	strcpy(_arg->arg, arg);
	list_add_tail(&(_arg->queue), head);
}

char * peek_arg(struct list_head *head)
{
	char * arg = NULL;
	struct cmd_arg *_arg;
	if (!list_empty(head)) {
		_arg = list_entry(head->next, struct cmd_arg, queue);
		arg = _arg->arg;
	}
	return arg;
}

void dequeue_arg(struct list_head *head)
{
	struct cmd_arg * _arg;
	if (!list_empty(head)) {
		_arg = list_entry(head->next, struct cmd_arg, queue);
		list_del(&(_arg->queue));
		free(_arg->arg);
		free(_arg);
	}
}

void clear_arg_list(struct list_head *head)
{
	while (!list_empty(head)) {
		dequeue_arg(head);
	}
}

void fill_arg_list(struct list_head *head, char * str)
{
	char * token = strtok(str, " \t");
	queue_arg(head, token);
	while ((token = strtok(NULL, " \t")) != NULL) {
		queue_arg(head, token);
	}
}

int _setpath(struct state *s, char* device, char *pathname, int mkdir)
{
	char *path, *p;
	int rsp, i = 0, j;
	path = xmalloc(strlen(device) + strlen(pathname) + 2);
	strcpy(path, device);
	strcat(path, "\\");
	strcat(path, pathname);
	p = path;
	while(p[0] != '\0') {
		while((p[0] == '/' || p[0] == '\\') &&
		      (p[1] == '/' || p[1] == '\\')) {
			char *t = p;
			while(t[0] != '\0') {
				t++;
				t[-1] = t[0];
			}
		}
		if (p[0] == '/')
			p[0] = '\\';
		p++;
	}
	rsp = exword_setpath(s->device, path, mkdir);
	if (rsp == 0x20) {
		free(s->cwd);
		s->cwd = xmalloc(strlen(path) + 1);
		strcpy(s->cwd, path);
	}
	free(path);
	return rsp;
}

void quit(struct state *s)
{
	s->running = 0;
	disconnect(s);
}

void help(struct state *s)
{
	int i;
	char *cmd = peek_arg(&(s->cmd_list));
	if (cmd == NULL) {
		for (i = 0; commands[i].cmd_str != NULL; i++) {
			if (commands[i].help_short != NULL)
				printf("%s", commands[i].help_short);
		}
	} else {
		for (i = 0; commands[i].cmd_str != NULL; i++) {
			if (strcmp(cmd, commands[i].cmd_str) == 0) {
				if (commands[i].help_long != NULL)
					printf("%s", commands[i].help_long);
				else
					printf("No help available for %s\n", cmd);
				break;
			}
		}
		if (commands[i].cmd_str == NULL)
			printf("%s is not a command\n", cmd);
	}
}

void connect(struct state *s)
{
	int  options = OPEN_LIBRARY | LOCALE_JA;
	int response;
	char *mode;
	char *locale;
	int error = 0;
	int i;
	uint16_t count;
	exword_dirent_t *entries;
	if (s->connected)
		return;

	mode = peek_arg(&(s->cmd_list));
	if (mode != NULL) {
		if (strcmp(mode, "library") == 0) {
			options = OPEN_LIBRARY;
		} else if (strcmp(mode, "text") == 0) {
			options = OPEN_TEXT;
		} else if (strcmp(mode, "text2") == 0) {
			options = OPEN_TEXT2;
		} else if (strcmp(mode, "cd") == 0) {
			options = OPEN_CD;
		} else {
			printf("Unknown 'type': %s\n", mode);
			error = 1;
		}
		dequeue_arg(&(s->cmd_list));
		locale = peek_arg(&(s->cmd_list));
		if (!error && locale != NULL) {
			if (strcmp(locale, "ja") == 0) {
				options |= LOCALE_JA;
			} else if (strcmp(locale, "kr") == 0) {
				options |= LOCALE_KR;
			} else if (strcmp(locale, "cn") == 0) {
				options |= LOCALE_CN;
			} else if (strcmp(locale, "de") == 0) {
				options |= LOCALE_DE;
			} else if (strcmp(locale, "es") == 0) {
				options |= LOCALE_ES;
			} else if (strcmp(locale, "fr") == 0) {
				options |= LOCALE_FR;
			} else if (strcmp(locale, "ru") == 0) {
				options |= LOCALE_RU;
			} else {
				printf("Unknown 'locale': %s\n", locale);
				error = 1;
			}
		} else if (!error) {
			options |= LOCALE_JA;
		}
	}
	if (!error) {
		obex_trace("CLI connect mode=%s locale=0x%02x options=0x%04x\n",
			(options & OPEN_TEXT2) ? "text2" : (options & OPEN_TEXT) ? "text" : (options & OPEN_CD) ? "cd" : "library",
			options & 0xff, options);
		printf("connecting to device...");
		s->device = exword_open2(options);
		if (s->device == NULL) {
			printf("device not found\n");
		} else {
			exword_set_debug(s->device, s->debug);
			response = exword_connect(s->device);
			obex_trace("CONNECT response=%d (0x%x)\n", response, response);
			if (response != 0x20) {
				printf("connect failed: %s (result %d / 0x%x)\n",
					exword_response_to_string(response), response, response);
				exword_close(s->device);
				s->device = NULL;
			} else {
				response = exword_setpath(s->device, ROOT, 0);
				obex_trace("initial SETPATH root response=%d (0x%x)\n", response, response);
				if (response == 0x20) {
					response = exword_list(s->device, &entries, &count);
					obex_trace("initial LIST response=%d (0x%x)\n", response, response);
					if (response == 0x20) {
						for (i = 0; i < count; i++) {
							if (strcmp(entries[i].name, "_SD_00") == 0) {
								s->sd_inserted = 1;
								break;
							}
						}
						exword_free_list(entries);
					}
				}
				response = _setpath(s, INTERNAL_MEM, "/", 0);
				obex_trace("initial SETPATH internal response=%d (0x%x)\n", response, response);
				s->connected = 1;
				s->mode = (options & OPEN_TEXT2) ? OPEN_TEXT : (options & 0xff00);
				printf("done\n");
			}
		}
	}
}

void disconnect(struct state *s)
{
	if (!s->connected)
		return;
	printf("disconnecting...");
	exword_disconnect(s->device);
	exword_close(s->device);
	free(s->cwd);
	s->cwd = NULL;
	s->device = NULL;
	s->connected = 0;
	s->authenticated = 0;
	printf("done\n");
}

void model(struct state *s)
{
	int rsp;
	exword_model_t model;
	if (!s->connected)
		return;
	rsp = exword_get_model(s->device, &model);
	s->last_response = rsp;
	if (rsp == 0x20) {
		printf("Model: %s\nSub: %s\n", model.model, model.sub_model);
		if (model.capabilities & CAP_EXT)
			printf("Extended: %s\n", model.ext_model);
		if ((model.capabilities & ~CAP_EXT) != 0) {
			printf("Capabilities: %s %s %s %s\n",
				model.capabilities & CAP_SW ? "SW" : "",
				model.capabilities & CAP_P ? "P" : "",
				model.capabilities & CAP_F ? "F" : "",
				model.capabilities & CAP_C ? "C" : "");
		}
	} else {
		printf("%s\n", exword_response_to_string(rsp));
	}
}

void capacity(struct state *s)
{
	int rsp;
	exword_capacity_t cap;
	if (!s->connected)
		return;
	rsp = exword_get_capacity(s->device, &cap);
	s->last_response = rsp;
	if (rsp == 0x20)
		printf("Capacity: %d / %d\n", cap.total, cap.free);
	else
		printf("%s\n", exword_response_to_string(rsp));
}

void format(struct state *s)
{
	int rsp;
	if (!s->connected)
		return;
	printf("Formatting SD Card...");
	rsp = exword_sd_format(s->device);
	printf("%s\n", exword_response_to_string(rsp));
}

void send(struct state *s)
{
	int rsp, len;
	char *buffer;
	char *filename;
	char *name = NULL;
	if (!s->connected)
		return;
	filename = peek_arg(&(s->cmd_list));
	if (filename == NULL) {
		printf("No file specified\n");
	} else {
		name = xmalloc(strlen(filename) + 1);
		strcpy(name, filename);
		printf("uploading...");
		rsp = read_file(name, &buffer, &len);
		if (rsp == 0x20)
			rsp = exword_send_file(s->device, basename(name), buffer, len);
		free(name);
		free(buffer);
		printf("%s\n", exword_response_to_string(rsp));
	}
}

void get(struct state *s)
{
	int rsp, len;
	char *buffer = NULL, *name = NULL;
	char *filename;
	if (!s->connected)
		return;
	filename = peek_arg(&(s->cmd_list));
	if (filename == NULL) {
		printf("No file specified\n");
	} else {
		name = xmalloc(strlen(filename) + 1);
		strcpy(name, filename);
		printf("downloading...");
		rsp = exword_get_file(s->device, basename(name), &buffer, &len);
		if (rsp == 0x20)
			rsp = write_file(filename, buffer, len);
		free(name);
		free(buffer);
		printf("%s\n", exword_response_to_string(rsp));
	}
}

void delete(struct state *s)
{
	int rsp;
	char * filename;
	if (!s->connected)
		return;
	filename = peek_arg(&(s->cmd_list));
	if (filename == NULL) {
		printf("No file specified\n");
	} else {
		printf("deleting file...");
		if (filename[0] == '*')
			rsp = exword_remove_file(s->device, filename + 1, 1);
		else
			rsp = exword_remove_file(s->device, filename, 0);
		printf("%s\n", exword_response_to_string(rsp));
	}
}

void dump(struct state *s)
{
	char *destination = peek_arg(&s->cmd_list);
	if (!s->connected || !s->cwd) { printf("Connect and select a device folder first.\n"); return; }
	if (!destination) { printf("Usage: dump NEW_LOCAL_DIRECTORY (no spaces)\n"); return; }
	/* Never retains a pointer into the mutable command queue or changes s->cwd. */
	s->last_response = exword_dump_tree(s->device, s->cwd, destination) ? 0x20 : -1;
}

void list(struct state *s)
{
	int rsp, i;
	char * name;
	int len;
	exword_dirent_t *entries;
	uint16_t count;
	if (!s->connected)
		return;
	rsp = exword_list(s->device, &entries, &count);
	s->last_response = rsp;
	if (rsp == 0x20) {
		for (i = 0; i < count; i++) {
			if (entries[i].flags & LIST_F_UNICODE) {
				utf16_to_locale(&name, &len,
						entries[i].name,
						entries[i].size - 3);
				if (entries[i].flags & LIST_F_DIR)
					printf("<*%s>\n", name);
				else
					printf("*%s\n", name);
			} else {
				if (entries[i].flags & LIST_F_DIR)
					printf("<%s>\n", entries[i].name);
				else
					printf("%s\n", entries[i].name);
			}
		}
		exword_free_list(entries);
	}
	printf("%s\n", exword_response_to_string(rsp));
}

int dict_list(exword_t *device, char *root);
int dict_remove(exword_t *device, char *root, char *id);
int dict_decrypt(exword_t *device, char *root, char *id);
int dict_install(exword_t *device, char *root, char *id);
int dict_auth(exword_t *device, char *user, char *auth);

void dict(struct state *s)
{
	int i;
	char val;
	char root[15];
	char authkey[41];
	char *subfunc, *id, *user;
	if (!s->connected)
		return;
	if (s->mode != OPEN_LIBRARY) {
		printf("Only available in library mode.\n");
		return;
	}
	if (!memcmp(s->cwd, "\\_SD_00", 7))
		strcpy(root, "\\_SD_00\\");
	else
		strcpy(root, "\\_INTERNAL_00\\");
	if (peek_arg(&(s->cmd_list)) == NULL) {
		printf("No sub-function specified.\n");
	} else {
		subfunc = xmalloc(strlen(peek_arg(&(s->cmd_list))) + 1);
		strcpy(subfunc, peek_arg(&(s->cmd_list)));
		dequeue_arg(&(s->cmd_list));
		if (strcmp(subfunc, "list") == 0) {
			dict_list(s->device, root);
		} else if (strcmp(subfunc, "reset") == 0) {
			if (peek_arg(&(s->cmd_list)) == NULL) {
				printf("No username specified.\n");
			} else {
				user = peek_arg(&(s->cmd_list));
				if (dict_reset(s->device, user))
					s->authenticated = 1;
				else
					s->authenticated = 0;
			}
		} else if (strcmp(subfunc, "auth") == 0) {
			if (peek_arg(&(s->cmd_list)) == NULL) {
				printf("No username specified.\n");
			} else {
				user = xmalloc(strlen(peek_arg(&(s->cmd_list))) + 1);
				strcpy(user, peek_arg(&(s->cmd_list)));
				dequeue_arg(&(s->cmd_list));
				if (peek_arg(&(s->cmd_list)) == NULL) {
					if (dict_auth(s->device, user, NULL)) {
						s->authenticated = 1;
						printf("Authentication sucessful.\n");
					} else {
						printf("Authentication failed.\n");
					}
				} else {
					if (sscanf(peek_arg(&(s->cmd_list)), "0x%40[a-fA-F0-9]", authkey) < 1) {
						printf("Invalid character in authkey.\n");
					} else if (strlen(authkey) != 40) {
						printf("Authkey wrong length. Must be 20 bytes.\n");
					} else {
						for (i = 0; i < 40; i += 2) {
							if (authkey[i] >= 97)
								val = (authkey[i] - 87) << 4;
							else if (authkey[i] >= 65)
								val = (authkey[i] - 55) << 4;
							else if (authkey[i] >= 48)
								val = (authkey[i] - 48) << 4;
							if (authkey[i + 1] >= 97)
								val |= (authkey[i + 1] - 87);
							else if (authkey[i + 1] >= 65)
								val |= (authkey[i + 1] - 55);
							else if (authkey[i + 1] >= 48)
								val |= (authkey[i + 1] - 48);
							authkey[i / 2] = val;
						}
						if (dict_auth(s->device, user, authkey)) {
							s->authenticated = 1;
							printf("Authentication sucessful.\n");
						} else {
							printf("Authentication failed.\n");
						}
					}
				}
			}
		} else if (strcmp(subfunc, "decrypt") == 0 ||
			   strcmp(subfunc, "remove")  == 0 ||
			   strcmp(subfunc, "install") == 0) {
			id = peek_arg(&(s->cmd_list));
			if (id == NULL) {
				printf("No id specified.\n");
			} else if (strlen(id) != 5) {
				printf("Id must be 5 characters long.\n");
			} else {
				if (!s->authenticated) {
					printf("Not authenticated.\n");
					return;
				}
				if (strcmp(subfunc, "decrypt") == 0)
					dict_decrypt(s->device, root, id);
				else if (strcmp(subfunc, "install") == 0)
					dict_install(s->device, root, id);
				else if (strcmp(subfunc, "remove") == 0)
					dict_remove(s->device, root, id);
			}
		} else {
			printf("Unknown subfunction\n");
		}
		if (exword_setpath(s->device, s->cwd, 0) != 0x20) {
			_setpath(s, root, "/", 0);
		}
		free(subfunc);
	}
}

void setpath(struct state *s)
{
	int rsp;
	char *path;
	char path2[256];
	if (!s->connected)
		return;
	path = peek_arg(&(s->cmd_list));
	if (path == NULL) {
		printf("No path specified\n");
	} else {
		rsp = sscanf(path, "sd://%255s", path2);
		if (rsp > 0) {
			if (s->sd_inserted) {
				rsp = _setpath(s, SD_CARD, path2, s->mkdir);
				if (rsp != 0x20) {
					printf("%s\n", exword_response_to_string(rsp));
					exword_setpath(s->device, s->cwd, 0);
				}
			} else {
				printf("SD card not inserted.\n");
			}
		} else {
			rsp = sscanf(path, "mem://%255s", path2);
			if (rsp > 0) {
				rsp = _setpath(s, INTERNAL_MEM, path2, s->mkdir);
				if (rsp != 0x20) {
					printf("%s\n", exword_response_to_string(rsp));
					exword_setpath(s->device, s->cwd, 0);
				}
			} else {
				printf("Invalid argument. Format (sd|mem)://<path>\n");
			}
		}
	}
}

void set(struct state *s)
{
	char * opt;
	char * arg;
	opt = peek_arg(&(s->cmd_list));
	if (opt == NULL) {
		printf("No option specified\n");
	} else if (strcmp(opt, "debug") == 0) {
		uint8_t debug;
		dequeue_arg(&(s->cmd_list));
		arg = peek_arg(&(s->cmd_list));
		if (arg == NULL) {
			printf("Debug Level: %u\n", s->debug);
		} else {
			if (sscanf(arg, "%hhu", &debug) < 1) {
				printf("Invalid value\n");
			} else if (debug > 5) {
				printf("Value should be between 0 and 5\n");
			} else {
				s->debug = debug;
				if (s->connected)
					exword_set_debug(s->device, s->debug);
			}
		}
	} else if (strcmp(opt, "mkdir") == 0) {
		dequeue_arg(&(s->cmd_list));
		arg = peek_arg(&(s->cmd_list));
		if (arg == NULL) {
			printf("Mkdir: %u\n", s->mkdir);
		} else {
			if (strcmp(arg, "on") == 0 ||
			    strcmp(arg, "yes") == 0 ||
			    strcmp(arg, "true") == 0) {
				s->mkdir = 1;
			} else if (strcmp(arg, "off") == 0 ||
			    strcmp(arg, "no") == 0 ||
			    strcmp(arg, "false") == 0) {
				s->mkdir = 0;
			} else {
				printf("Invalid value\n");
			}
		}
	} else {
		printf("Unknown option %s\n", opt);
	}
}

void process_command(struct state *s)
{
	int i;
	char * cmd = peek_arg(&(s->cmd_list));
	for (i = 0; commands[i].cmd_str != NULL; i++) {
		if (strcmp(cmd, commands[i].cmd_str) == 0) {
			dequeue_arg(&(s->cmd_list));
			commands[i].ptr(s);
			break;
		}
	}
	if (commands[i].cmd_str == NULL)
		printf("Unknown command\n");
}

char * create_prompt(char *cwd) {
	char *p;
	if (cwd == NULL) {
		p = xmalloc(4);
		strcpy(p, ">> ");
	} else {
		p = xmalloc(strlen(cwd) + 5);
		strcpy(p, cwd);
		strcat(p, " >> ");
	}
	return p;
}

void interactive(struct state *s)
{
	char * line = NULL;
	char * prompt = NULL;
	printf("Exword dictionary tool.\n"
	       "Type 'help' for a list of commands.\n");
	s->running = 1;
	INIT_LIST_HEAD(&(s->cmd_list));
	load_history();
	while (s->running) {
		free(line);
		free(prompt);
		prompt = create_prompt(s->cwd);
		line = readline(prompt);
		if (line == NULL) {
			quit(s);
			break;
		}
		if (*line == '\0')
			continue;
		add_history(line);
		fill_arg_list(&(s->cmd_list), line);
		process_command(s);
		clear_arg_list(&(s->cmd_list));
	}
	free(line);
	free(prompt);
	store_history();
}

/* Deliberately allow only inspection operations, not arbitrary command scripts. */
static int query_options(const char *mode, const char *region, int *options)
{
	static const char *regions[] = {"ja", "kr", "cn", "de", "es", "fr", "ru"};
	static const int locales[] = {LOCALE_JA, LOCALE_KR, LOCALE_CN, LOCALE_DE,
		LOCALE_ES, LOCALE_FR, LOCALE_RU};
	int i, flags;
	if (!strcmp(mode, "library")) flags = OPEN_LIBRARY;
	else if (!strcmp(mode, "text")) flags = OPEN_TEXT;
	else if (!strcmp(mode, "text2")) flags = OPEN_TEXT2;
	else return 0;
	for (i = 0; i < (int)(sizeof(locales) / sizeof(locales[0])); ++i) {
		if (!strcmp(region, regions[i])) {
			*options = flags | locales[i];
			return 1;
		}
	}
	return 0;
}

static int run_query(struct state *s, int options, const char *name)
{
	int response, disconnect_response;
	obex_trace("QUERY name=%s options=0x%04x BEGIN\n", name, options);
	s->device = exword_open2(options);
	if (!s->device) {
		fprintf(stderr, "Unable to open USB device; inspect discovery/driver log.\n");
		return 1;
	}
	exword_set_debug(s->device, s->debug);
	response = exword_connect(s->device);
	printf("CONNECT: %s (result %d / 0x%x)\n",
		exword_response_to_string(response), response, response);
	obex_trace("QUERY CONNECT result=%d (0x%x)\n", response, response);
	if (response == 0x50) {
		printf("Device returned D0: profile mismatch OR device session/storage preflight failure.\n"
		       "USB ACK/response means this is not a transport timeout.\n");
	}
	if (response == 0x20) {
		s->connected = 1;
		s->last_response = response;
		if (!strcmp(name, "capacity") || !strcmp(name, "list")) {
			/* exword_setpath accepts a boolean: false encodes no-create (02). */
			response = _setpath(s, INTERNAL_MEM, "/", 0);
			printf("SETPATH internal: %s (0x%x)\n", exword_response_to_string(response), response);
		}
		if (response == 0x20) {
			if (!strcmp(name, "model")) model(s);
			else if (!strcmp(name, "capacity")) capacity(s);
			else if (!strcmp(name, "list")) list(s);
			response = s->last_response;
		}
		disconnect_response = exword_disconnect(s->device);
		printf("DISCONNECT: %s (0x%x)\n",
			exword_response_to_string(disconnect_response), disconnect_response);
		if (response == 0x20) response = disconnect_response;
	}
	exword_close(s->device);
	s->device = NULL;
	s->connected = 0;
	free(s->cwd);
	s->cwd = NULL;
	obex_trace("QUERY name=%s result=%d (0x%x) END\n", name, response, response);
	return response == 0x20 ? 0 : 1;
}

int main(int argc, const char** argv)
{
	struct state s;
	int i;
	const char *usb_log = NULL;
	const char *query = NULL;
	int options = 0;
	setlocale(LC_ALL, "");
	memset(&s, 0, sizeof(struct state));
	for (i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			obex_verbose = 1;
		} else if (!strcmp(argv[i], "--keep-alt")) {
			obex_keep_alt = 1;
		} else if (!strcmp(argv[i], "--casio-init")) {
			obex_casio_init = 1;
		} else if (!strcmp(argv[i], "--query")) {
			if (query || i + 3 >= argc ||
			    !query_options(argv[i + 1], argv[i + 2], &options)) {
				fprintf(stderr, "Use --query library|text|text2 ja|kr|cn|de|es|fr|ru handshake|model|capacity|list\n");
				return 2;
			}
			query = argv[i + 3];
			if (strcmp(query, "handshake") && strcmp(query, "model") &&
			    strcmp(query, "capacity") && strcmp(query, "list")) {
				fprintf(stderr, "Query must be handshake, model, capacity or list (read-only).\n");
				return 2;
			}
			i += 3;
		} else if (!strcmp(argv[i], "--usb-log")) {
			if (++i == argc) {
				fprintf(stderr, "--usb-log requires a filename\n");
				return 2;
			}
			usb_log = argv[i];
			obex_verbose = 1;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf("Usage: %s [-v|--verbose] [--usb-log FILE] [--keep-alt] [--casio-init]\n"
			       "Verbose mode dumps USB discovery, transfers and protocol diagnostics.\n"
			       "--usb-log writes diagnostics to FILE (overwrites); commands stay interactive.\n"
			       "--keep-alt skips redundant selection of USB alternate setting zero.\n"
			       "--casio-init sends the vendor initialization used by CESG502.sys.\n", argv[0]);
			printf("--query MODE REGION NAME runs one read-only query and exits.\n"
			       "  MODE: library, text, text2; REGION: ja, kr, cn, de, es, fr, ru\n"
			       "  NAME: handshake, model, capacity, list\n"
			       "  handshake sends only CONNECT and, if accepted, DISCONNECT.\n"
			       "  Exit codes: 0 success, 1 device/protocol failure, 2 invalid arguments.\n"
			       "Example: exword --keep-alt --casio-init --usb-log cy436-cn.log --query library cn handshake\n");
			return 0;
		} else {
			fprintf(stderr, "Unknown option: %s (use --help)\n", argv[i]);
			return 2;
		}
	}
	if (usb_log && !freopen(usb_log, "w", stderr)) {
		perror("Unable to open USB log");
		return 2;
	}
	if (obex_verbose) {
		s.debug = 5;
		obex_trace("libexword-re verbose USB capture started; timestamps are Unix seconds\n");
	}
	if (query) return run_query(&s, options, query);
	interactive(&s);
	return 0;
}
