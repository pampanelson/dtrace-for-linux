/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)pr_getrctl.c	1.1	01/04/05 SMI"

#define	_LARGEFILE64_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include "libproc.h"

/*
 * getrctl() system call -- executed by subject process
 */
int
pr_getrctl(struct ps_prochandle *Pr, const char *rname,
	rctlblk_t *old_blk, rctlblk_t *new_blk, int rflag)
{
	sysret_t rval;
	argdes_t argd[6];
	argdes_t *adp;
	int error;

	if (Pr == NULL)		/* no subject process */
		return (getrctl(rname, old_blk, new_blk, rflag));

	adp = &argd[0];
	adp->arg_value = 0;	/* switch for getrctl in rctlsys */
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;
	adp->arg_value = 0;
	adp->arg_object = (void *)rname;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(rname) + 1;

	adp++;
	if (old_blk == NULL) {
		adp->arg_value = 0;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		adp->arg_value = 0;
		adp->arg_object = old_blk;
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = rctlblk_size();
	}

	adp++;
	if (new_blk == NULL) {
		adp->arg_value = 0;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_OUTPUT;
		adp->arg_size = 0;
	} else {
		adp->arg_value = 0;
		adp->arg_object = new_blk;
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_INOUT;
		adp->arg_size = rctlblk_size();
	}

	adp++;
	adp->arg_value = 0;		/* obufsz isn't used by getrctl() */
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;
	adp->arg_value = rflag;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	error = Psyscall(Pr, &rval, SYS_rctlsys, 6, &argd[0]);

	if (error) {
		errno = (error > 0) ? error : ENOSYS;
		return (-1);
	}
	return (rval.sys_rval1);
}

/*
 * setrctl() system call -- executed by subject process
 */
int
pr_setrctl(struct ps_prochandle *Pr, const char *rname,
	rctlblk_t *old_blk, rctlblk_t *new_blk, int rflag)
{
	sysret_t rval;
	argdes_t argd[6];
	argdes_t *adp;
	int error;

	if (Pr == NULL)		/* no subject process */
		return (setrctl(rname, old_blk, new_blk, rflag));

	adp = &argd[0];
	adp->arg_value = 1;	/* switch for setrctl in rctlsys */
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;
	adp->arg_value = 0;
	adp->arg_object = (void *)rname;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = strlen(rname) + 1;

	adp++;
	if (old_blk == NULL) {
		adp->arg_value = 0;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		adp->arg_value = 0;
		adp->arg_object = old_blk;
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = rctlblk_size();
	}

	adp++;
	if (new_blk == NULL) {
		adp->arg_value = 0;
		adp->arg_object = NULL;
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		adp->arg_value = 0;
		adp->arg_object = new_blk;
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = rctlblk_size();
	}

	adp++;
	adp->arg_value = 0;		/* obufsz isn't used by setrctl() */
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;
	adp->arg_value = rflag;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	error = Psyscall(Pr, &rval, SYS_rctlsys, 6, &argd[0]);

	if (error) {
		errno = (error > 0) ? error : ENOSYS;
		return (-1);
	}
	return (rval.sys_rval1);
}
