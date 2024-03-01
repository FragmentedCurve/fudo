/*
 * Copyright 2023 Paco Pascal
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <assert.h>

#define alloca(x) __builtin_alloca(x)

extern char **environ;

struct arglist {
	char **args;
	size_t cap;
	size_t len;
};

void arglist_init(struct arglist *al) {
	al->cap = 1024;
	al->args = malloc(sizeof(char*) * (al->cap));
	assert(al->args);
	// There's always a null for execv & friends
	al->len = 1;
	al->args[0] = NULL;
}

void arglist_insert(struct arglist *al, char *s) {
	if (al->len == al->cap) {
		al->cap *= 2;
		al->args = realloc(al->args, (al->cap) * sizeof(char*));
		assert(al->args);
	}

	al->args[al->len - 1] = s;
	al->args[al->len] = NULL;
	al->len++;
}

enum {
	UNKNOWN,
	PASS,    // Pass argument to doas.
	IGNORE,  // Ignore argument
	EXIT     // Don't execute doas.
};

struct option {
	char *sudo;
	char *doas;
	int attr;
	int nargs;
} equiv[] = {
	{ "-A",                 0,    IGNORE, 0 },
	{ "--askpass",          0,    IGNORE, 0 },
	{ "-B",                 0,    IGNORE, 0 },
	{ "--bell",             0,    IGNORE, 0 },
	{ "-b",                 "-n", PASS,   0 }, // Run in background
	{ "--background",       "-n", PASS,   0 }, // Run in background
	{ "-C",                 0,    IGNORE, 1 },
	{ "--close-from=",      0,    IGNORE, 0 },
	{ "-D",                 0,    IGNORE, 1 },
	{ "--chdir=",           0,    IGNORE, 0 },
	{ "-E",                 0,    IGNORE, 0 },
	{ "--preserve-env",     0,    IGNORE, 0 },
	{ "--preserve-env=",    0,    IGNORE, 0 },
	{ "-e",                 0,    EXIT,   0 },
	{ "--edit",             0,    EXIT,   0 },
	{ "-g",                 0,    IGNORE, 1 },
	{ "--group=",           0,    IGNORE, 0 },
	{ "-H",                 0,    IGNORE, 0 },
	{"--set-home",          0,    IGNORE, 0 },	
	{ "--help",             0,    IGNORE, 0 },
	{ "-h",                 0,    IGNORE, 1 },
	{ "--host=",            0,    IGNORE, 0 },
	{ "-i",                 "-s", PASS,   0 }, // Execute user's shell
	{ "-K",                 0,    IGNORE, 0 },
	{ "--remove-timestamp", 0,    IGNORE, 0 },
	{ "-k",                 0,    IGNORE, 0 },
	{ "--reset-timestamp",  0,    IGNORE, 0 },
	{ "-l",                 0,    IGNORE, 0 },
	{ "--list",             0,    IGNORE, 0 },
	{ "-N",                 0,    IGNORE, 0 },
	{ "--no-update",        0,    IGNORE, 0 },
	{ "-n",                 "-n", PASS,   0 }, // Run in background
	{ "--non-interactive",  "-n", PASS,   0 }, // Run in background
	{ "-P",                 0,    IGNORE, 0 },
	{ "--preserve-groups",  0,    IGNORE, 0 },
	{ "-p",                 0,    IGNORE, 1 },
	{ "--prompt=",          0,    IGNORE, 0 }, 
	{ "-R",                 0,    IGNORE, 1 },
	{ "--chroot=",          0,    IGNORE, 0 }, 
	{ "-S",                 0,    IGNORE, 0 },
	{ "--stdin",            0,    IGNORE, 0 },
	{ "-s",                 "-s", PASS,   0 }, // Execute user's shell
	{ "--shell",            "-s", PASS,   0 }, // Execute user's shell
	{ "-U",                 0,    IGNORE, 1 },
	{ "--other-user=",      0,    IGNORE, 0 },
	{ "-T",                 0,    IGNORE, 1 },
	{ "--command-timeout=", 0,    IGNORE, 0 },
	{ "-u",                 "-u", PASS,   1 }, // Run as user
	{ "--user=",            "-u", PASS,   0 }, // Run as user
	{ "-V",                 0,    IGNORE, 0 },
	{ "--version",          0,    IGNORE, 0 },
	{ "-v",                 0,    IGNORE, 0 },
	{ "--validate",         0,    IGNORE, 0 },

	// --

	{ NULL }
};

struct option find_equiv(char *arg) {
	for (int i = 0; equiv[i].sudo; i++)
		if (!strncmp(equiv[i].sudo, arg, strlen(equiv[i].sudo)))
			return equiv[i];

	// arg isn't a sudo arg so just pass it to doas
	return (struct option) { arg, 0, UNKNOWN, 0 };
}

char *which(const char *prog) {
	char *epath = getenv("PATH");	
	size_t epath_bytes = strlen(epath) + 1;
	char *path = alloca(epath_bytes);
	char *ppath = alloca(1);
	size_t ppath_bytes = 1;
	
	// Clone $PATH to the stack
	memcpy(path, epath, epath_bytes);

	// Empty string
	*ppath = 0;
	
	while ((path = strtok(path, ":"))) {
		// Buf size of "%s/%s"
		size_t needs = strlen(path) + strlen(prog) + 2;
			
		// Allocate more space if current buffer is too small
		if (ppath_bytes < needs) {
			ppath = alloca(needs - ppath_bytes);
			ppath_bytes = needs;
		}

		snprintf(ppath, ppath_bytes, "%s/%s", path, prog);
		
		// If we can execute ppath, dup and return it
		if (!access(ppath, R_OK | X_OK))
			return strdup(ppath);
		
		path = NULL;
	}

	// 404 not found
	return NULL;
}

void show_cmd(char *prefix, int argc, char **argv) {
	if (getenv("FUDO_HIDE"))
		return;
	
	char *sep = "";
	fprintf(stderr, "%s", prefix);
	for (int i = 0; i < argc; i++) {
		fprintf(stderr, "%s%s", sep, argv[i]);
		sep = " ";
	}
	fprintf(stderr, "\n");
}

char *parse_long_flag(struct option opt, char *arg) {
	size_t len = strlen(opt.sudo);

	// Is it a long form flag?
	if (opt.sudo[len - 1] != '=')
		return NULL;
	
	return arg + len;
}

int main(int argc, char **argv) {
	struct arglist al; // doas arguments
	char *doas_prog = which("doas");
	
	if (!doas_prog) {
		fprintf(stderr, "doas couldn't be found on your system.\n");
		return -1;
	}

	arglist_init(&al);
	arglist_insert(&al, doas_prog);
	
	// Display inputed sudo command
	show_cmd("fudo <<< ", argc, argv);

	{ // Setup Environment
		char buf[128];
		uid_t uid = getuid();
		struct passwd *pw = getpwuid(uid);
		
		setenv("SUDO_USER", pw->pw_name, 1);
		snprintf(buf, sizeof(buf), "%d", pw->pw_uid);
		setenv("SUDO_UID", buf, 1);
		snprintf(buf, sizeof(buf), "%d", pw->pw_gid);
		setenv("SUDO_GID", buf, 1);
	}
	
	{ // Process sudo arguments
		int passthru = 0;
		for (size_t i = 1; i < argc; i++) {
			if (passthru) {
				arglist_insert(&al, argv[i]);
				continue;
			}

			if (!strcmp("--", argv[i])) {
				passthru = 1;
				continue;
			}
	
			struct option opt = find_equiv(argv[i]);
			if (opt.attr == UNKNOWN) {
				passthru = 1;
				arglist_insert(&al, argv[i]);
				continue;
			}

			switch (opt.attr) {
			case PASS: {
				if (opt.doas)
					arglist_insert(&al, opt.doas);
				else
					arglist_insert(&al, opt.sudo);

				char *l = parse_long_flag(opt, argv[i]);
				if (l) {
					// It's a long flag, such as --user=root
					arglist_insert(&al, l);
				} else if (opt.nargs) {
					// Flag has arguments that follow it
					i++;
					for (size_t j = 0; j < opt.nargs && i < argc; j++) {
						i += j;
						arglist_insert(&al, argv[i]);
					}
				}
			} break;
			case IGNORE:
				break;
			case EXIT:
				exit(0);
			}
			
		}
	}

	// Display the translated doas command
	show_cmd("fudo >>> ", al.len - 1, al.args);
	
	execve(doas_prog, al.args, environ);

	// Never reached
	abort();
}
