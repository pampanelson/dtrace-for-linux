/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#ifndef	_SYS_FASTTRAP_H
#define	_SYS_FASTTRAP_H

//#pragma ident	"@(#)fasttrap.h	1.3	04/08/31 SMI"

#include <sys/fasttrap_isa.h>
#include <sys/dtrace.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	FASTTRAPIOC		(('m' << 24) | ('r' << 16) | ('f' << 8))
#define	FASTTRAPIOC_MAKEPROBE	(FASTTRAPIOC | 1)
#define	FASTTRAPIOC_GETINSTR	(FASTTRAPIOC | 2)

typedef enum fasttrap_probe_type {
	DTFTP_NONE = 0,
	DTFTP_ENTRY,
	DTFTP_RETURN,
	DTFTP_OFFSETS,
	DTFTP_POST_OFFSETS,
	DTFTP_IS_ENABLED
} fasttrap_probe_type_t;

typedef struct fasttrap_probe_spec {
	pid_t			ftps_pid;
	fasttrap_probe_type_t	ftps_type;

	char			ftps_func[DTRACE_FUNCNAMELEN];
	char			ftps_mod[DTRACE_MODNAMELEN];

	uint64_t		ftps_pc;
	uint64_t		ftps_size;
	uint64_t		ftps_noffs;
	uint64_t		ftps_offs[1];
} fasttrap_probe_spec_t;

typedef struct fasttrap_instr_query {
	uint64_t		ftiq_pc;
	pid_t			ftiq_pid;
	fasttrap_instr_t	ftiq_instr;
} fasttrap_instr_query_t;

/*
 * To support the fasttrap provider from very early in a process's life,
 * the run-time linker, ld.so.1, has a program header of type PT_SUNWDTRACE
 * which points to a data object which must be PT_SUNWDTRACE_SIZE bytes.
 * This structure mimics the fasttrap provider section of the ulwp_t structure.
 * When the fasttrap provider is changed to require new or different
 * instructions, the data object in ld.so.1 and the thread initializers in libc
 * (libc_init() and _thrp_create()) need to be updated to include the new
 * instructions, and PT_SUNWDTRACE needs to be changed to a new unique number
 * (while the old value gets assigned something like PT_SUNWDTRACE_1). Since the
 * linker must be backward compatible with old Solaris releases, it must have
 * program headers for each of the PT_SUNWDTRACE versions. The kernel's
 * elfexec() function only has to look for the latest version of the
 * PT_SUNWDTRACE program header.
 */
#define	PT_SUNWDTRACE_SIZE	FASTTRAP_SUNWDTRACE_SIZE

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FASTTRAP_H */
