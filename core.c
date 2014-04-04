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
static struct filter **
start_plugins(const char *dirpath)
{
	DIR *dirp;
	struct dirent *dp;
	struct filter **plugins;
	size_t null = 0;

	/* First, allocate the null entry */
	plugins = malloc(sizeof(struct filter *));
	if (!plugins) {
		ERROR("failed to allocate the null entry");
		goto out;
	}

	plugins[null] = NULL;

	/* This allows scripts with relative paths to library and such to work as expected */
	DBG("change working directory to '%s'", dirpath);
	if (chdir(dirpath)) {
		ERRORX("chdir");
		goto out;
	}

	dirp = opendir(".");
	if (dirp == NULL) {
		ERRORX("opendir '.' ('%s')", dirpath);
		goto out;
	}

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
			struct filter *plugin;
			void *reloc;

			DBG("file '%s' is executable, forking...", dp->d_name);

			reloc = realloc(plugins, sizeof(struct filter *) * (null + 1 + 1));
			if (!reloc) {
				ERROR("failed to reallocate memory for '%s'", dp->d_name);
				continue;
			}
			plugins = reloc;

			plugin = filter_exec(argv, SIG_PLUGOUT, SIG_PLUGERR);
			if (!plugin) {
				ERROR("failed to spawn '%s'", dp->d_name);
				continue;
			}

			plugins[null++] = plugin;
			plugins[null] = NULL;
			DBG("sucessfully forked '%s'", dp->d_name);
		}
	}

	if (errno != 0)
		ERRORX("readdir");

	if (closedir(dirp) == -1)
		ERRORX("closedir");
out:
	return plugins;
}

/* Kill all plugins */
static int
stop_plugins(struct filter **plugins)
{
	int n, err = 0;

	/* Free each plugin */
	for (n = 0; plugins[n] != NULL; ++n) {
		/* put the plugins in a group and send the signal to the group instead */
		DBG("core: sending SIGTERM to plugin %d", plugins[n]->pid);
		if (kill(plugins[n]->pid, SIGTERM)) {
			ERRORX("kill plugin %d", plugins[n]->pid);
			err++;
		}
		filter_free(plugins[n]);
	}

	/* Free the list */
	free(plugins);

	return err;
}

#ifdef DEBUG
static const char *
plugin_name(struct filter **plugins, pid_t pid)
{
	int n;

	for (n = 0; plugins[n] != NULL; ++n)
		if (plugins[n]->pid == pid)
			return plugins[n]->name;

	/* Unlikely */
	return NULL;
}
#endif /* DEBUG */

static ssize_t
map_instreams(struct filter **plugins, int **instreams)
{
	ssize_t n = 0;

	/* Count number of plugins */
	while (plugins[n] != NULL)
		n++;

	if (n > 0) {
		int i;

		/* Allocate and fill the array */
		*instreams = malloc(sizeof(int) * n);
		if (!*instreams)
			return -1;

		for (i = 0; i < n; ++i)
			(*instreams)[i] = plugins[i]->in;
	}

	DBG("%zi plugins input streams mapped", n);
	return n;
}

int main(int argc, char *argv[])
{
	const char *plugindir;
	struct filter *backend, **plugins;
	int *instreams, errstreams[2] = { STDERR_FILENO };
	size_t nin, nerr = 1;
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
			nerr = 2;
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
		char *filter_argv[n];
		int i;

		for (i = 0; i < n; ++i)
			filter_argv[i] = argv[optind + i];
		filter_argv[i] = NULL;

		DBG("starting back end '%s'...", filter_argv[0]);
		backend = filter_exec(filter_argv, SIG_BACKOUT, SIG_BACKERR);
		if (!backend) {
			ERROR("modulo: failed to run back end\nTry '%s -h' for more information.", filter_argv[0]);
			return 1;
		}

		/* Store the backend stdin as an error stream for debug mode */
		errstreams[1] = backend->in;
	}

	/* List, fork and execute plugins */
	plugins = start_plugins(plugindir);
	if (!plugins) {
		ERROR("an error occured while starting plugins");
		goto stop_backend;
	}
	nin = map_instreams(plugins, &instreams);
	if (nin == -1) {
		ERROR("failed to map plugins stdin");
		goto stop_plugins;
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
		case SIGHUP:
			/* XXX need to clear the queued signals? */
			DBG("reloading plugins...");
			ret = stop_plugins(plugins);
			if (ret) {
				ERROR("an error occured while stopping plugins");
				goto stop_backend;
			}
			plugins = start_plugins(plugindir);
			if (!plugins) {
				ERROR("an error occured while starting plugins");
				ret = 1;
				goto stop_backend;
			}
			free(instreams);
			nin = map_instreams(plugins, &instreams);
			if (nin == -1) {
				ERROR("failed to map plugins stdin");
				ret = 1;
				goto stop_plugins;
			}
			break;
		case SIGUSR1:
			nerr = (nerr % 2) + 1;
			DBG("debug mode %s", nerr == 2 ? "enabled" : "disabled");
			break;
		case SIG_BACKERR:
		case SIG_PLUGERR:
			/*
			 * TODO put plugins in a group, stop_plugins() will
			 * kill() the group, thus no need to store their pid,
			 * struct filter will be obsolete and start plugins()
			 * could take a stdin streams array and return the
			 * number of plugins.
			 */
			DBG("[%s:%d] ~> error stream", siginfo.si_pid == backend->pid ? backend->name : plugin_name(plugins, siginfo.si_pid), siginfo.si_pid);
			if (forward_to_many(siginfo.si_fd, errstreams, nerr))
				ERROR("failed to push to error stream(s)");
			break;
		case SIG_PLUGOUT:
			DBG("[%s:%d] ~> back end", plugin_name(plugins, siginfo.si_pid), siginfo.si_pid);
			if (forward(siginfo.si_fd, backend->in))
				ERROR("failed to push %d output to back end", siginfo.si_pid);
			break;
		case SIG_BACKOUT:
			DBG("[%s:%d] ~> plugins", backend->name, siginfo.si_pid);
			if (forward_to_many(siginfo.si_fd, instreams, nin))
				ERROR("failed to push back end output to plugins");
			break;
		default:
			DBG("ignored signal %d (%s)", sig, strsignal(sig));
			break;
		}
	}

stop_plugins:
	DBG("exit: stop plugins...");
	if (stop_plugins(plugins))
		ERROR("an error occured while stopping plugins");
stop_backend:
	DBG("exit: stop backend...");
	if (kill(backend->pid, SIGTERM))
		ERRORX("kill back end %d", backend->pid);
	filter_free(backend);

	return ret;
}
