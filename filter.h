/*
 * filter.h - header for filter manipulation
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

#ifndef _FILTER_H
#define _FILTER_H

struct filter {
#ifdef DEBUG
	char *name;
#endif /* DEBUG */
	pid_t pid;		/* process ID */
	pid_t gid;		/* process group ID */
	int in;			/* standard input stream */
};

void filter_free(struct filter *filter);
struct filter *filter_exec(char *argv[], int sigout, int sigerr);

#endif /* _FILTER_H */
