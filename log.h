/*
 * log.h - error and debugging macros
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
 *
 *
 * This header file defines the following printf-like macros:
 *
 * DBG(format, ...)    - print a debug message on STDERR
 * INFO(format, ...)   - print a message on STDOUT
 * ERROR(format, ...)  - print an error message on STDERR
 * ERRORX(format, ...) - like ERROR() but with the errno reason
 *
 * If DEBUG is defined at compile time, DBG() is enabled and all macros print
 * file and line info, otherwise DBG() is disabled (being a noop).
 */

#ifndef _LOG_H
#define _LOG_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef DEBUG

#define __PRINT(fd, prefix, format, ...) \
	dprintf(fd, "%d " prefix " %s:%d: " format "\n", (int) time(NULL), __func__, __LINE__, ##__VA_ARGS__)

#define DBG(format, ...) \
	__PRINT(STDERR_FILENO, "DEBUG", format, ##__VA_ARGS__)

#else /* DEBUG undefined */

#define __PRINT(fd, prefix, format, ...) \
	dprintf(fd, prefix " " format "\n", ##__VA_ARGS__)

#define DBG(format, ...) \
	do { } while (0)

#endif /* DEBUG */

#define INFO(format, ...) \
	__PRINT(STDOUT_FILENO, "INFO", format, ##__VA_ARGS__)

#define ERROR(format, ...) \
	__PRINT(STDERR_FILENO, "ERROR", format, ##__VA_ARGS__)

#define ERRORX(format, ...) \
	ERROR(format ": %s", ##__VA_ARGS__, strerror(errno))

#endif /* _LOG_H */
