/* $OpenBSD$ */

/*
 * Copyright (c) 2015 Olof-Joachim Frahm <olof@macrolet.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "tmux.h"

/*
 * Load status prompt history from a file.
 */

enum cmd_retval	 cmd_load_status_history_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_load_status_history_entry = {
	"load-history", "loadh",
	":a", 0, 1,
	"[-a] [path]",
	0,
	cmd_load_status_history_exec
};

enum cmd_retval
cmd_load_status_history_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct client	*c = cmdq->client;
	struct session	*s;
	struct passwd	*pw;
	const char	*home;
	FILE		*f = NULL;
	char		*line = NULL;
	size_t		 length;
	ssize_t		 read = 0;
	char		*path = NULL;
	int		 cwd, fd, append;
	u_int		 i;

	append = args_has(self->args, 'a');

	if (args->argc == 1) {
		path = args->argv[0];

		if (c != NULL && c->session == NULL)
			cwd = c->cwd;
		else if ((s = cmd_find_current(cmdq)) != NULL)
			cwd = s->cwd;
		else
			cwd = AT_FDCWD;

		fd = openat(cwd, path, O_RDONLY, 0600);
	} else {
		home = getenv("HOME");

		if (home == NULL || *home == '\0') {
			pw = getpwuid(getuid());
			if (pw != NULL)
				home = pw->pw_dir;
			else
				home = NULL;
		}

		if (home == NULL) {
			cmdq_error(cmdq, "can't find home directory");
			return (CMD_RETURN_ERROR);
		}

		xasprintf(&path, "%s/.tmux.history", home);
		fd = open(path, O_CREAT|O_RDONLY, 0600);
	}

	if (fd != -1)
		f = fdopen(fd, "rb");

	if (f == NULL) {
		if (fd != -1)
			close(fd);
		cmdq_error(cmdq, "can't open %s for reading: %s",
			   path, strerror(errno));
		if (args->argc == 0)
			free(path);
		return (CMD_RETURN_ERROR);
	}

	if (!append) {
		for (i = 0; i < status_prompt_hsize && read >= 0;) {
			if ((read = getline(&line, &length, f)) > 1) {
				line[read - 1] = '\0';
				status_prompt_hlist[i++] = xstrdup(line);
			}
		}

		for (; status_prompt_hsize > i; status_prompt_hsize--)
			free(status_prompt_hlist[status_prompt_hsize - 1]);

		if (status_prompt_hsize > 0)
			xreallocarray(status_prompt_hlist, status_prompt_hsize,
				      sizeof *status_prompt_hlist);
		else {
			free(status_prompt_hlist);
			status_prompt_hlist = NULL;
		}
	}

	for (; read >= 0;) {
		if ((read = getline(&line, &length, f)) > 1) {
			line[read - 1] = '\0';
			status_prompt_add_history(line);
		}
	}

	free(line);
	fclose(f);
	return (CMD_RETURN_NORMAL);
}
