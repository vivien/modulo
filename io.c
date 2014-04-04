/*
 * io.c - non-blocking read and write operations
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

#define _GNU_SOURCE /* for F_SETSIG */

#include <fcntl.h>
#include <unistd.h>

#include "log.h"

int
eventio(int fd, int sig)
{
	int flags;

	DBG("setting %d as \"I/O possible\" signal for fd %d", sig, fd);

	if (fcntl(fd, F_SETSIG, sig) == -1) {
		ERRORX("fcntl F_SETSIG on fd %d", fd);
		return 1;
	}

	/* Set owner process that is to receive "I/O possible" signal */
	if (fcntl(fd, F_SETOWN, getpid()) == -1) {
		ERRORX("failed to set process as owner of fd %d", fd);
		return 1;
	}

	/* Enable "I/O possible" signaling and make I/O nonblocking */
	flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		ERRORX("fcntl F_GETFL on fd %d", fd);
		return 1;
	}

	if (fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1) {
		ERRORX("failed to enable I/O signaling for fd %d", fd);
		return 1;
	}

	return 0;
}

/*
 * Read at most size bytes to buf from the non-blocking pipe end fd.
 * Return the number of bytes read on success, -1 otherwise.
 */
static ssize_t
read_nonblock(int fd, char *buf, size_t size)
{
	ssize_t nr;

	nr = read(fd, buf, size);
	if (nr == -1) {
		if (errno == EAGAIN) {
			/* no more reading */
			return 0;
		}

		ERRORX("read from %d", fd);
		return -1;
	}

	if (nr == 0) {
		ERROR("pipe end not open for writing");
		return -1;
	}

	return nr;
}

/*
 * Write at most size bytes from buf to the non-blocking pipe end fd.
 * Return the number of bytes written on success, -1 otherwise.
 */
static ssize_t
write_nonblock(int fd, const char *buf, size_t size)
{
	ssize_t nw;

	/* TODO write in while loop? */
	/* TODO handle EPIPE and EAGAIN? */
	nw = write(fd, buf, size);
	if (nw != size) {
		if (nw == -1) {
			if (errno == EPIPE) {
				ERROR("pipe end closed");
				return -1;
			}

			ERRORX("write to %d", fd);
			return -1;
		}

		ERROR("wrote %zu bytes instead of %zu", nw, size);
		return -1;
	}

	if (nw == 0) {
		ERROR("XXX what does that mean?");
		return -1;
	}

	return nw;
}

/*
 * Read src fd and write it to the n fd in dests.
 */
int
forward_to_many(int src, int *dests, size_t n)
{
	/* XXX change 1024 for PIP_BUF? */
	char line[1024];
	ssize_t nr, nw;
	int i;

	while (nr = read_nonblock(src, line, sizeof(line)), nr > 0)
		for (i = 0; i < n; ++i)
			if (nw = write_nonblock(dests[i], line, nr), nw != nr)
				ERROR("write(%d) %zi/%zi", dests[i], nw, nr);

	return nr;
}

/*
 * Read src fd and write it to dest fd.
 * TODO make forward() be forward_to_many(src, &dest, 1) instead?
 */
int
forward(int src, int dest)
{
	/* XXX change 1024 for PIP_BUF? */
	char line[1024];
	ssize_t nr, nw;

	DBG("forwarding %d to %d", src, dest);
	while (nr = read_nonblock(src, line, sizeof(line)), nr > 0)
		if (nw = write_nonblock(dest, line, nr), nw != nr)
			return nw; /* might be 0? return 1 instead? */

	return nr;
}
