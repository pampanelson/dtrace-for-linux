/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#ifndef	_DT_PROC_H
#define	_DT_PROC_H

#pragma ident	"@(#)dt_proc.h	1.3	04/10/22 SMI"

#include <libproc.h>
#include <dtrace.h>
#include <pthread.h>
#include <dt_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_proc {
	dt_list_t dpr_list;		/* prev/next pointers for lru chain */
	struct dt_proc *dpr_hash;	/* next pointer for pid hash chain */
	struct dt_proc *dpr_notify;	/* next pointer for notification list */
	dtrace_hdl_t *dpr_hdl;		/* back pointer to libdtrace handle */
	struct ps_prochandle *dpr_proc;	/* proc handle for libproc calls */
	rd_agent_t *dpr_rtld;		/* rtld handle for librtld_db calls */
	pthread_mutex_t dpr_lock;	/* lock for manipulating dpr_hdl */
	pthread_cond_t dpr_cv;		/* cond for dpr_stop/quit/done */
	pid_t dpr_pid;			/* pid of process */
	uint_t dpr_refs;		/* reference count */
	uint8_t dpr_cacheable;		/* cache handle using lru list */
	uint8_t dpr_stop;		/* stop mask: see flag bits below */
	uint8_t dpr_quit;		/* quit flag: ctl thread should quit */
	uint8_t dpr_done;		/* done flag: ctl thread has exited */
	uint8_t dpr_usdt;		/* usdt flag: usdt initialized */
	pthread_t dpr_tid;		/* control thread (or zero if none) */
	dt_list_t dpr_bps;		/* list of dt_bkpt_t structures */
} dt_proc_t;

#define	DT_PROC_STOP_IDLE	0x01	/* idle on owner's stop request */
#define	DT_PROC_STOP_CREATE	0x02	/* wait on dpr_cv at process exec */
#define	DT_PROC_STOP_GRAB	0x04	/* wait on dpr_cv at process grab */
#define	DT_PROC_STOP_PREINIT	0x08	/* wait on dpr_cv at rtld preinit */
#define	DT_PROC_STOP_POSTINIT	0x10	/* wait on dpr_cv at rtld postinit */
#define	DT_PROC_STOP_MAIN	0x20	/* wait on dpr_cv at a.out`main() */

typedef void dt_bkpt_f(dt_proc_t *, void *);

typedef struct dt_bkpt {
	dt_list_t dbp_list;		/* prev/next pointers for bkpt list */
	dt_bkpt_f *dbp_func;		/* callback function to execute */
	void *dbp_data;			/* callback function private data */
	uintptr_t dbp_addr;		/* virtual address of breakpoint */
	ulong_t dbp_instr;		/* saved instruction from breakpoint */
	ulong_t dbp_hits;		/* count of breakpoint hits for debug */
	int dbp_active;			/* flag indicating breakpoint is on */
} dt_bkpt_t;

typedef struct dt_proc_hash {
	pthread_mutex_t dph_lock;	/* lock protecting dph_notify list */
	pthread_cond_t dph_cv;		/* cond for waiting for dph_notify */
	dt_proc_t *dph_notify;		/* list of pending proc notifications */
	dt_list_t dph_lrulist;		/* list of dt_proc_t's in lru order */
	uint_t dph_lrulim;		/* limit on number of procs to hold */
	uint_t dph_lrucnt;		/* count of cached process handles */
	uint_t dph_hashlen;		/* size of hash chains array */
	dt_proc_t *dph_hash[1];		/* hash chains array */
} dt_proc_hash_t;

extern struct ps_prochandle *dt_proc_create(dtrace_hdl_t *,
    const char *, char *const *);

extern struct ps_prochandle *dt_proc_grab(dtrace_hdl_t *, pid_t, int, int);
extern void dt_proc_release(dtrace_hdl_t *, struct ps_prochandle *);
extern void dt_proc_continue(dtrace_hdl_t *, struct ps_prochandle *);
extern void dt_proc_lock(dtrace_hdl_t *, struct ps_prochandle *);
extern void dt_proc_unlock(dtrace_hdl_t *, struct ps_prochandle *);
extern dt_proc_t *dt_proc_lookup(dtrace_hdl_t *, struct ps_prochandle *, int);

extern void dt_proc_hash_create(dtrace_hdl_t *);
extern void dt_proc_hash_destroy(dtrace_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROC_H */
