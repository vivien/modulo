/*
 * fork_exec.c - spawn a process and pipe its three standard streams
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

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

static int
dup_and_close(int old, int new)
{
	/* Defensive check */
	if (old == new)
		return 0;

	if (dup2(old, new) == -1) {
		ERRORX("dup2(%d, %d)", old, new);
		return 1;
	}

	if (close(old) == -1) {
		ERRORX("close(%d)", old);
		return 1;
	}

	return 0;
}

/*
 * Fork and exec a vector command line.
 * Return -1 on error, the child pid otherwise.
 */
pid_t
fork_exec(char *const argv[], int io[3])
{
	int in[2], out[2], err[2];
	pid_t pid;

	if (pipe(in) == -1) {
		ERRORX("pipe stdin");
		return -1;
	}

	if (pipe(out) == -1) {
		ERRORX("pipe stdout");
		return -1;
	}

	if (pipe(err) == -1) {
		ERRORX("pipe stderr");
		return -1;
	}

	pid = fork();
	switch (pid) {
	case -1:
		ERRORX("fork");
		return -1;
	case 0:
		/* Child, the filter process */

		/* Close unused pipe ends */
		if (close(in[1]) == -1) {
			ERRORX("close stdin write end");
			exit(1);
		}

		if (close(out[0]) == -1) {
			ERRORX("close stdout read end");
			exit(1);
		}

		if (close(err[0]) == -1) {
			ERRORX("close stderr read end");
			exit(1);
		}

		/* Connect stdin on read end of stdin pipe */
		if (dup_and_close(in[0], STDIN_FILENO))
			exit(1);

		/* Connect stdout on write end of stdout pipe */
		if (dup_and_close(out[1], STDOUT_FILENO))
			exit(1);

		/* Connect stderr on write end of stderr pipe */
		if (dup_and_close(err[1], STDERR_FILENO))
			exit(1);

		/* Finally execute the program */
		execv(argv[0], argv);

		/* We should not reach this point */
		ERRORX("execv %s", argv[0]);
		exit(1);
	default:
		/* Parent */

		/* Close unused pipe ends */
		if (close(in[0]) == -1) {
			ERRORX("close stdin read end");
			return -1;
		}

		if (close(out[1]) == -1) {
			ERRORX("close stdout write end");
			return -1;
		}

		if (close(err[1]) == -1) {
			ERRORX("close stderr write end");
			return -1;
		}

		io[0] = in[1]; /* Filter's stdin */
		io[1] = out[0]; /* Filter's stdout */
		io[2] = err[0]; /* Filter's stderr */
		break;
	}

	return pid;
}
