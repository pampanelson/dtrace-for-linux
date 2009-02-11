/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)proc_get_info.c	1.4	04/09/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "libproc.h"

/*
 * These several routines simply get the indicated /proc structures
 * for a process identified by process ID.  They are convenience
 * functions for one-time operations.  They do the mechanics of
 * open() / read() / close() of the necessary /proc files so the
 * caller's code can look relatively less cluttered.
 */

/*
 * 'ngroups' is the number of supplementary group entries allocated in
 * the caller's cred structure.  It should equal zero or one unless extra
 * space has been allocated for the group list by the caller, like this:
 *    credp = malloc(sizeof (prcred_t) + (ngroups - 1) * sizeof (gid_t));
 */
int
proc_get_cred(pid_t pid, prcred_t *credp, int ngroups)
{
	char fname[64];
	int fd;
	int rv = -1;
	ssize_t minsize = sizeof (*credp) - sizeof (gid_t);
	size_t size = minsize + ngroups * sizeof (gid_t);

	(void) snprintf(fname, sizeof (fname), "/proc/%d/cred", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (read(fd, credp, size) >= minsize)
			rv = 0;
		(void) close(fd);
	}
	return (rv);
}

/*
 * Malloc and return a properly sized structure.
 */
prpriv_t *
proc_get_priv(pid_t pid)
{
	char fname[64];
	int fd;
	struct stat statb;
	prpriv_t *rv = NULL;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/priv", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (fstat(fd, &statb) != 0 ||
		    (rv = malloc(statb.st_size)) == NULL ||
		    read(fd, rv, statb.st_size) != statb.st_size) {
			free(rv);
			rv = NULL;
		}
		(void) close(fd);
	}
	return (rv);
}

#if defined(__i386) || defined(__amd64)
/*
 * Fill in a pointer to a process LDT structure.
 * The caller provides a buffer of size 'nldt * sizeof (struct ssd)';
 * If pldt == NULL or nldt == 0, we return the number of existing LDT entries.
 * Otherwise we return the actual number of LDT entries fetched (<= nldt).
 */
int
proc_get_ldt(pid_t pid, struct ssd *pldt, int nldt)
{
	char fname[64];
	int fd;
	struct stat statb;
	size_t size;
	ssize_t ssize;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/ldt", (int)pid);
	if ((fd = open(fname, O_RDONLY)) < 0)
		return (-1);

	if (pldt == NULL || nldt == 0) {
		nldt = 0;
		if (fstat(fd, &statb) == 0)
			nldt = statb.st_size / sizeof (struct ssd);
		(void) close(fd);
		return (nldt);
	}

	size = nldt * sizeof (struct ssd);
	if ((ssize = read(fd, pldt, size)) < 0)
		nldt = -1;
	else
		nldt = ssize / sizeof (struct ssd);

	(void) close(fd);
	return (nldt);
}
#endif	/* __i386 || __amd64 */

int
proc_get_psinfo(pid_t pid, psinfo_t *psp)
{
	char fname[64];
	int fd;
	int rv = -1;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/psinfo", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (read(fd, psp, sizeof (*psp)) == sizeof (*psp))
			rv = 0;
		(void) close(fd);
	}
	return (rv);
}

int
proc_get_status(pid_t pid, pstatus_t *psp)
{
	char fname[64];
	int fd;
	int rv = -1;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/status", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (read(fd, psp, sizeof (*psp)) == sizeof (*psp))
			rv = 0;
		(void) close(fd);
	}
	return (rv);
}

/*
 * Get the process's aux vector.
 * 'naux' is the number of aux entries in the caller's buffer.
 * We return the number of aux entries actually fetched from
 * the process (less than or equal to 'naux') or -1 on failure.
 */
int
proc_get_auxv(pid_t pid, auxv_t *pauxv, int naux)
{
	char fname[64];
	int fd;
	int rv = -1;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/auxv", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if ((rv = read(fd, pauxv, naux * sizeof (auxv_t))) >= 0)
			rv /= sizeof (auxv_t);
		(void) close(fd);
	}
	return (rv);
}
