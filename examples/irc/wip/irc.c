/*
 * IRC filter - forward STDIN to a chan and output processed messages to STDOUT
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

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* XXX ensure SIGRTMIN == 42 */

struct irc {
	/* connection info */
	const char *server;
	const char *chan;
	const char *nick;
	int port;

	/* internal usage */
	int sockfd;
};

static int
create_socket(struct irc *irc)
{
	struct sockaddr_in host_sa;
	struct hostent *host;

	/* Create the Internet stream (TCP) socket */
	irc->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (irc->sockfd == -1) {
		perror("socket");
		return -1;
	}

	/* Get server info */
	host = gethostbyname(irc->server);
	if (!host) {
		fprintf(stderr, "no such host\n");
		return -1;
	}

	/* Address to bind to the socket */
	memset(&host_sa, 0, sizeof(host_sa));
	host_sa.sin_family = AF_INET;
	memcpy(&host_sa.sin_addr.s_addr, host->h_addr, host->h_length);
	host_sa.sin_port = htons(irc->port);

	/* Establish a connection to the server */
	if (connect(irc->sockfd, (struct sockaddr *) &host_sa, sizeof(host_sa)) == -1) {
		perror("connect");
		return -1;
	}

	return 0;
}

static int
join(struct irc *irc)
{
#if 0
	fprintf(fp, "PASS %s\n", );
#endif
	write(irc->sockfd, "USER v1n 0 * :v1n\n", 18);
	write(irc->sockfd, "NICK v1n\n", 9);
	write(irc->sockfd, "JOIN #vivien\n", 13);

	/* XXX check for error */
	return 0;
}

static int
irc_connect(struct irc *irc)
{
	if (create_socket(irc)) {
		fprintf(stderr, "failed to create a socket to %s:%d\n", irc->server, irc->port);
		return 1;
	}

	if (join(irc)) {
		fprintf(stderr, "failed to join %s\n", irc->chan);
		return 1;
	}

	return 0;
}

static void
irc_disconnect(struct irc *irc)
{
	close(irc->sockfd);
}

static int
forward(int src, int dest)
{
	char buf[1024];
	ssize_t nr;

	/* XXX use dprintf(fd, "PRIVMSG :%s"...); */

	while (nr = read(src, buf, sizeof(buf)), nr > 0)
		if (write(dest, buf, nr) != nr)
			return 1;

	return 0;
}

#define debug(...) ;
#define error(...) ;
#define errorx(...) ;

/*
 * TODO add setup_eventio
 */

int main(int argc, char *argv[])
{
	int c;
	struct irc irc = {
		.server = "irc.freenode.net",
		.nick = "modulo",
		.port = 6667,
	};

	siginfo_t siginfo;
	sigset_t sigset;
	int sig;

	while (c = getopt(argc, argv, "s:p:c:n:h"), c != -1) {
		switch (c) {
		case 's':
			irc.server = optarg;
			break;
		case 'p':
			irc.port = atoi(optarg);
			break;
		case 'c':
			irc.chan = optarg;
			break;
		case 'n':
			irc.nick = optarg;
			break;
		case 'h':
			fprintf(stdout, "Usage: %s [-h] [-s <server>] [-p <port>] [-c <chan>] [-n <nick>]\n", argv[0]);
			return 0;
		default:
			/* Unkown key, doesn't matter. */
			break;
		}
	}

	if (!irc.chan) {
		fprintf(stderr, "You should provide a channel to connect to.\n");
		return 1;
	}

	if (irc_connect(&irc)) {
		fprintf(stderr, "failed to connect");
		return 1;
	}

	/* Block all signals (except SIGKILL and SIGSTOP) */
	sigfillset(&sigset);
	if (sigprocmask(SIG_SETMASK, &sigset, NULL) == -1) {
		errorx("sigprocmask");
		return 1;
	}
	debug("signals blocked");

	/* Fetch signals until SIGINT (^C) or SIGTERM */
	for (;;) {
		sig = sigwaitinfo(&sigset, &siginfo);
		debug("got signal %d (%s)", sig, strsignal(sig));

		switch (sig) {
		case -1:
			errorx("sigwaitinfo");
			return 1;
		case SIGINT:
		case SIGTERM:
			debug("%s received, exiting.", strsignal(sig));
			return 0;
		case SIGIO:
			/* XXX handle it */
			error("signal-queue overflow!");
			return 1;
		case 42:
			forward(STDIN_FILENO, irc.sockfd);
			break;
		case 43:
			forward(irc.sockfd, STDOUT_FILENO);
			break;
		default:
			debug("si_signo=%d, si_code=%d (%s), si_value=%d",
					siginfo.si_signo, siginfo.si_code,
					(siginfo.si_code == SI_USER) ? "SI_USER" :
					(siginfo.si_code == SI_QUEUE) ? "SI_QUEUE" : "other",
					siginfo.si_value.sival_int);
			debug("si_pid=%ld, si_uid=%ld", (long) siginfo.si_pid, (long) siginfo.si_uid);
			break;
		}
	}

	irc_disconnect(&irc);
	return 0;
}
