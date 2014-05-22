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

int
filter_exec(char *argv[], int sigout, int sigerr)
{
	int io[3];
	pid_t pid;

	pid = fork_exec(argv, io);
	if (pid == -1) {
		ERROR("failed to fork filter %s", argv[0]);
		return -1;
	}

	if (eventio(io[1], sigout)) {
		ERROR("failed to set event I/O for filter stdout");
		return -1;
	}

	if (eventio(io[2], sigerr)) {
		ERROR("failed to set event I/O for filter stderr");
		return -1;
	}

	DBG("[%s:%d] in:%d out:%d err:%d", strrchr(argv[0], '/') ? : argv[0],
			pid, io[0], io[1], io[2]);

	/*
	 * No need to store the filter stdout and stderr, they will be given to
	 * us by the kernel on I/O possible signal. Just return stdin pipe end.
	 */

	return io[0];
}
