/*
 * core.c - the I/O multiplexer
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

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "filter.h"
#include "io.h"
#include "log.h"

#ifndef VERSION
#define VERSION "unknown"
#endif

/* Lowest value is prioritary */
#if 0
#define SIG_BACKERR	SIGRTMIN+0
#define SIG_PLUGERR	SIGRTMIN+1
#define SIG_PLUGOUT	SIGRTMIN+2
#define SIG_BACKOUT	SIGRTMIN+3
#else
#define SIG_BACKERR	42
#define SIG_PLUGERR	43
#define SIG_PLUGOUT	44
#define SIG_BACKOUT	45
#endif
/* XXX add a check for the value of SIGRTMIN? */

/*
 * Find every plugins in the directory 'dirpath' and execute them.
 * Return the filters in a null-terminated array
 *
 * TODO get dir realpath in main() then chdir()
 */
static ssize_t
start_plugins(const char *dirpath, int **plugfds, size_t *nfds)
{
	DIR *dirp;
	struct dirent *dp;

	/* This allows scripts with relative paths to library and such to work as expected */
	DBG("change working directory to '%s'", dirpath);
	if (chdir(dirpath)) {
		ERRORX("chdir");
		return 1;
	}

	dirp = opendir(".");
	if (dirp == NULL) {
		ERRORX("opendir '.' ('%s')", dirpath);
		return 1;
	}

	*plugfds = NULL;
	*nfds = 0;

	/* For each entry in this directory, fork/exec any executable */
	for (;;) {
		struct stat statbuf;

		errno = 0; /* to distinguish error from end-of-directory */
		dp = readdir(dirp);
		if (dp == NULL)
			break;

		/* XXX open() then fstat() instead? */
		if (stat(dp->d_name, &statbuf) == -1) {
			ERRORX("failed to get stat for '%s'", dp->d_name);
			continue;
		}

		if (!S_ISREG(statbuf.st_mode)) {
			DBG("skip '%s'", dp->d_name);
			continue;
		}

		/*
		 * The time gap between the access() call and the fork is not
		 * really an issue, since the exec() call will check again.
		 */
		if (access(dp->d_name, X_OK) == 0) {
			char *argv[2] = { dp->d_name };
			void *reloc;
			int infd;

			DBG("file '%s' is executable, forking...", dp->d_name);

			reloc = realloc(*plugfds, sizeof(int) * (*nfds + 1));
			if (!reloc) {
				ERROR("failed to reallocate memory for '%s'", dp->d_name);
				continue;
			}
			*plugfds = reloc;

			infd = filter_exec(argv, SIG_PLUGOUT, SIG_PLUGERR);
			if (infd == -1) {
				ERROR("failed to spawn '%s'", dp->d_name);
				continue;
			}

			(*plugfds)[(*nfds)++] = infd;
			DBG("sucessfully forked '%s'", dp->d_name);
		}
	}

	if (errno != 0)
		ERRORX("readdir");

	if (closedir(dirp) == -1)
		ERRORX("closedir");

	return 0;
}

static void
wait_nonblock(void)
{
	for (;;) {
		siginfo_t infop = { 0 };
		pid_t pid;
		int code;

		/* Non-blocking check for dead child(ren) */
		if (waitid(P_ALL, 0, &infop, WEXITED | WNOHANG) == -1)
			if (errno != ECHILD)
				ERRORX("waitid");

		/* Error or no (dead yet) child(ren) */
		pid = infop.si_pid;
		if (pid == 0)
			break;

		code = WEXITSTATUS(infop.si_status);
		if (code != 0)
			ERROR("process %d exited with status %d", pid, code);
		else
			DBG("process %d exited correctly", pid);
	}
}

static void
stop_plugins(int *plugfds, size_t nfds)
{
	int i;

	for (i = 0; i < nfds; ++i)
		if (close(plugfds[i]) == -1)
			ERRORX("close(%d)", plugfds[i]);
	free(plugfds);
}

int main(int argc, char *argv[])
{
	const char *plugindir;

	int *plugfds, errfds[2] = { STDERR_FILENO };
	size_t nplugfds, nerrfds = 1;
	int backfd;

	int ret;
	int c;

	siginfo_t siginfo;
	sigset_t sigset;
	int sig;

	/* Option processing stops as soon as a nonoption argument is encountered */
	while (c = getopt(argc, argv, "+hvd"), c != -1) {
		switch (c) {
		case 'h':
			dprintf(STDOUT_FILENO, "Usage: modulo [-h] [-v] [-d] <plugins-directory> <back end> [<back-end-option>...]\n");
			return 0;
		case 'v':
			dprintf(STDOUT_FILENO, "modulo " VERSION " © 2014 Vivien Didelot\n");
			return 0;
		case 'd':
			nerrfds = 2;
			break;
		default:
			ERROR("Try 'modulo -h' for more information.");
			return 1;
		}
	}

	if (argc - optind < 2) {
		ERROR("modulo: missing operand\nTry 'modulo -h' for more information.");
		return 1;
	}

	plugindir = argv[optind++];

	/* Block all signals (except SIGKILL and SIGSTOP) */
	sigfillset(&sigset);
	if (sigprocmask(SIG_SETMASK, &sigset, NULL) == -1) {
		ERRORX("sigprocmask");
		return 1;
	}
	DBG("signals blocked");

	/* Start the back end */
	{
		const size_t n = argc - optind + 1;
		char *backargv[n];
		int i;

		for (i = 0; i < n; ++i)
			backargv[i] = argv[optind + i];
		backargv[i] = NULL;

		DBG("starting back end '%s'...", backargv[0]);
		backfd = filter_exec(backargv, SIG_BACKOUT, SIG_BACKERR);
		if (backfd == -1) {
			ERROR("modulo: failed to run back end\nTry '%s -h' for more information.", backargv[0]);
			return 1;
		}

		/* Store the backend stdin as an error stream for debug mode */
		errfds[1] = backfd;
	}

	/* List, fork and execute plugins */
	ret = start_plugins(plugindir, &plugfds, &nplugfds);
	if (ret) {
		ERROR("an error occured while starting plugins");
		goto stop_backend;
	}

	/* Fetch signals until SIGINT (^C) or SIGTERM */
	for (;;) {
		sig = sigwaitinfo(&sigset, &siginfo);
		if (sig == -1) {
			ERRORX("sigwaitinfo");
			ret = 1;
			break;
		}

		DBG("si_signo=%d, si_code=%d (%s), si_value=%d, si_pid=%ld, si_uid=%ld",
				siginfo.si_signo,
				siginfo.si_code,
				(siginfo.si_code == SI_USER) ? "SI_USER" :
				(siginfo.si_code == SI_QUEUE) ? "SI_QUEUE" : "other",
				siginfo.si_value.sival_int,
				(long) siginfo.si_pid,
				(long) siginfo.si_uid);

		switch (sig) {
		case SIGINT:
		case SIGTERM:
			DBG("exiting.");
			ret = 0;
			goto stop_plugins;
		case SIGIO:
			/* XXX handle it */
			ERROR("signal-queue overflow!");
			ret = 1;
			goto stop_plugins;
		case SIGCHLD:
			wait_nonblock();
			break;
		case SIGHUP:
			/* XXX need to clear the queued signals? */
			DBG("reloading plugins...");
			stop_plugins(plugfds, nplugfds);
			ret = start_plugins(plugindir, &plugfds, &nplugfds);
			if (ret) {
				ERROR("an error occured while starting plugins");
				goto stop_backend;
			}
			free(plugfds);
			break;
		case SIGUSR1:
			nerrfds = (nerrfds % 2) + 1;
			DBG("debug mode %s", nerrfds == 2 ? "enabled" : "disabled");
			break;
		case SIG_BACKERR:
		case SIG_PLUGERR:
			if (forward_to_many(siginfo.si_fd, errfds, nerrfds))
				ERROR("failed to push to error stream(s)");
			break;
		case SIG_PLUGOUT:
			if (forward(siginfo.si_fd, backfd))
				ERROR("failed to push %d output to back end", siginfo.si_pid);
			break;
		case SIG_BACKOUT:
			if (forward_to_many(siginfo.si_fd, plugfds, nplugfds))
				ERROR("failed to push back end output to plugins");
			break;
		default:
			DBG("ignored signal %d (%s)", sig, strsignal(sig));
			break;
		}
	}

stop_plugins:
	DBG("exit: stop plugins...");
	stop_plugins(plugfds, nplugfds);
stop_backend:
	DBG("exit: stop backend...");
	if (close(backfd) == -1)
		ERRORX("close(%d)", backfd);

	/* wait for child processes termination */
	/* TODO flush pipes? */
	/* TODO unblock SIGINT/SIGTERM? */
	DBG("waiting for child(ren) to terminate...");
	while (waitpid(-1, NULL, 0) > 0)
		continue;

	return ret;
}
