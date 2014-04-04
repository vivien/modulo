/*
 * filter.c - pipe a standalone program
 * Copyright (C) 2014 Vivien Didelot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "io.h"
#include "log.h"

extern pid_t fork_exec(char *const argv[], int io[3]);

void
filter_free(struct filter *filter)
{
#ifdef DEBUG
	free(filter->name);
#endif /* DEBUG */
	free(filter);
}

#ifdef DEBUG
static char *
rstrip_slash(const char *str)
{
	char *slash = strrchr(str, '/');
	return strdup(slash ? slash + 1 : str);
}
#endif /* DEBUG */

struct filter *
filter_exec(char *argv[], int sigout, int sigerr)
{
	struct filter *filter;
	int io[3];

	filter = malloc(sizeof(struct filter));
	if (!filter) {
		ERROR("failed to allocate memory");
		goto out;
	}

#ifdef DEBUG
	filter->name = rstrip_slash(argv[0]);
#endif /* DEBUG */

	filter->pid = fork_exec(argv, io);
	if (filter->pid == -1) {
		ERROR("failed to fork filter %s", argv[0]);
		goto free;
	}

	filter->in = io[0];

	if (eventio(io[1], sigout)) {
		ERROR("failed to set event I/O for filter stdout");
		goto kill;
	}

	if (eventio(io[2], sigerr)) {
		ERROR("failed to set event I/O for filter stderr");
		goto kill;
	}

	DBG("[%s:%d] in:%d out:%d err:%d", argv[0], filter->pid, io[0], io[1], io[2]);

	/*
	 * No need to store the filter stdout and stderr, they will be given to
	 * us by the kernel on I/O possible signal.
	 */

	goto out;

kill:
	/* TODO */
free:
	filter_free(filter);
	filter = NULL;
out:
	return filter;
}
