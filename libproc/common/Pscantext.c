/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)Pscantext.c	1.6	04/09/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libproc.h"
#include "Pcontrol.h"
#include "Pisadep.h"
#include "Putil.h"

#define	BLKSIZE	(8 * 1024)

/*
 * Look for a SYSCALL instruction in the process's address space.
 */
int
Pscantext(struct ps_prochandle *P)
{
	char mapfile[100];
	int mapfd;
	off_t offset;		/* offset in text section */
	off_t endoff;		/* ending offset in text section */
	uintptr_t sysaddr;	/* address of SYSCALL instruction */
	int syspri;		/* priority of SYSCALL instruction */
	int nbytes;		/* number of bytes in buffer */
	int n2bytes;		/* number of bytes in second buffer */
	int nmappings;		/* current number of mappings */
	prmap_t *pdp;		/* pointer to map descriptor */
	prmap_t *prbuf;		/* buffer for map descriptors */
	unsigned nmap;		/* number of map descriptors */
	uint32_t buf[2 * BLKSIZE / sizeof (uint32_t)];	/* text buffer */
	uchar_t *p;

	/* try the most recently-seen syscall address */
	syspri = 0;
	sysaddr = 0;
	if (P->sysaddr != 0 &&
	    (syspri = Pissyscall(P, P->sysaddr)))
		sysaddr = P->sysaddr;

	/* try the previous instruction */
	if (sysaddr == 0 || syspri != 1)
		syspri = Pissyscall_prev(P, P->status.pr_lwp.pr_reg[R_PC],
		    &sysaddr);

	if (sysaddr != 0 && syspri == 1) {
		P->sysaddr = sysaddr;
		return (0);
	}

	/* open the /proc/<pid>/map file */
	(void) sprintf(mapfile, "/proc/%d/map", (int)P->pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0) {
		dprintf("failed to open %s: %s\n", mapfile, strerror(errno));
		return (-1);
	}

	/* allocate a plausible initial buffer size */
	nmap = 50;

	/* read all the map structures, allocating more space as needed */
	for (;;) {
		prbuf = malloc(nmap * sizeof (prmap_t));
		if (prbuf == NULL) {
			dprintf("Pscantext: failed to allocate buffer\n");
			(void) close(mapfd);
			return (-1);
		}
		nmappings = pread(mapfd, prbuf, nmap * sizeof (prmap_t), 0L);
		if (nmappings < 0) {
			dprintf("Pscantext: failed to read map file: %s\n",
			    strerror(errno));
			free(prbuf);
			(void) close(mapfd);
			return (-1);
		}
		nmappings /= sizeof (prmap_t);
		if (nmappings < nmap)	/* we read them all */
			break;
		/* allocate a bigger buffer */
		free(prbuf);
		nmap *= 2;
	}
	(void) close(mapfd);

	/*
	 * Scan each executable mapping looking for a syscall instruction.
	 * In dynamically linked executables, syscall instructions are
	 * typically only found in shared libraries.  Because shared libraries
	 * are most often mapped at the top of the address space, we minimize
	 * our expected search time by starting at the last mapping and working
	 * our way down to the first mapping.
	 */
	for (pdp = &prbuf[nmappings - 1]; sysaddr == 0 && syspri != 1 &&
	    pdp >= prbuf; pdp--) {

		offset = (off_t)pdp->pr_vaddr;	/* beginning of text */
		endoff = offset + pdp->pr_size;

		/* avoid non-EXEC mappings; avoid the stack and heap */
		if ((pdp->pr_mflags&MA_EXEC) == 0 ||
		    (endoff > P->status.pr_stkbase &&
		    offset < P->status.pr_stkbase + P->status.pr_stksize) ||
		    (endoff > P->status.pr_brkbase &&
		    offset < P->status.pr_brkbase + P->status.pr_brksize))
			continue;

		(void) lseek(P->asfd, (off_t)offset, 0);

		if ((nbytes = read(P->asfd, buf, 2*BLKSIZE)) <= 0)
			continue;

		if (nbytes < BLKSIZE)
			n2bytes = 0;
		else {
			n2bytes = nbytes - BLKSIZE;
			nbytes  = BLKSIZE;
		}

		p = (uchar_t *)buf;

		/* search text for a SYSCALL instruction */
		while (sysaddr == 0 && syspri != 1 && offset < endoff) {
			if (nbytes <= 0) {	/* shift buffers */
				if ((nbytes = n2bytes) <= 0)
					break;
				(void) memcpy(buf,
					&buf[BLKSIZE / sizeof (buf[0])],
					nbytes);
				n2bytes = 0;
				p = (uchar_t *)buf;
				if (nbytes == BLKSIZE &&
				    offset + BLKSIZE < endoff)
					n2bytes = read(P->asfd,
						&buf[BLKSIZE / sizeof (buf[0])],
						BLKSIZE);
			}

			if (syspri = Pissyscall_text(P, p, nbytes))
				sysaddr = offset;

			p += sizeof (instr_t);
			offset += sizeof (instr_t);
			nbytes -= sizeof (instr_t);
		}
	}

	if ((P->sysaddr = sysaddr) != 0)
		return (0);
	else
		return (-1);
}
