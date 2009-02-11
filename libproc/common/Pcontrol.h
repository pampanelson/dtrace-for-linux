/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#ifndef	_PCONTROL_H
#define	_PCONTROL_H

#pragma ident	"@(#)Pcontrol.h	1.15	04/09/28 SMI"

/*
 * Implemention-specific include file for libproc process management.
 * This is not to be seen by the clients of libproc.
 */

#include <stdio.h>
#include <gelf.h>
#include <synch.h>
#include <procfs.h>
#include <rtld_db.h>
#include <libproc.h>
#include <libctf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include "Putil.h"

# if linux
#define PF_SUNW_FAILURE 0x00100000      /* mapping absent due to failure */
# endif

/*
 * Definitions of the process control structures, internal to libproc.
 * These may change without affecting clients of libproc.
 */

typedef struct {		/* symbol table */
	Elf_Data *sym_data;	/* start of table */
	size_t	sym_symn;	/* number of entries */
	char	*sym_strs;	/* ptr to strings */
	size_t	sym_strsz;	/* size of string table */
	GElf_Shdr sym_hdr;	/* symbol table section header */
	GElf_Shdr sym_strhdr;	/* string table section header */
	Elf	*sym_elf;	/* faked-up elf handle from core file */
	void	*sym_elfmem;	/* data for faked-up elf handle */
	uint_t	*sym_byname;	/* symbols sorted by name */
	uint_t	*sym_byaddr;	/* symbols sorted by addr */
	size_t	sym_count;	/* number of symbols in each sorted list */
} sym_tbl_t;

typedef struct file_info {	/* symbol information for a mapped file */
	list_t	file_list;	/* linked list */
	char	file_pname[PRMAPSZ];	/* name from prmap_t */
	struct map_info *file_map;	/* primary (text) mapping */
	int	file_ref;	/* references from map_info_t structures */
	int	file_fd;	/* file descriptor for the mapped file */
	int	file_init;	/* 0: initialization yet to be performed */
	GElf_Half file_etype;	/* ELF e_type from ehdr */
	GElf_Half file_class;	/* ELF e_ident[EI_CLASS] from ehdr */
	rd_loadobj_t *file_lo;	/* load object structure from rtld_db */
	char	*file_lname;	/* load object name from rtld_db */
	char	*file_lbase;	/* pointer to basename of file_lname */
	Elf	*file_elf;	/* elf handle so we can close */
	void	*file_elfmem;	/* data for faked-up elf handle */
	sym_tbl_t file_symtab;	/* symbol table */
	sym_tbl_t file_dynsym;	/* dynamic symbol table */
	uintptr_t file_dyn_base;	/* load address for ET_DYN files */
	uintptr_t file_plt_base;	/* base address for PLT */
	size_t	file_plt_size;	/* size of PLT region */
	uintptr_t file_jmp_rel;	/* base address of PLT relocations */
	uintptr_t file_ctf_off;	/* offset of CTF data in object file */
	size_t	file_ctf_size;	/* size of CTF data in object file */
	int	file_ctf_dyn;	/* does the CTF data reference the dynsym */
	void	*file_ctf_buf;	/* CTF data for this file */
	ctf_file_t *file_ctfp;	/* CTF container for this file */
} file_info_t;

typedef struct map_info {	/* description of an address space mapping */
	prmap_t	map_pmap;	/* /proc description of this mapping */
	file_info_t *map_file;	/* pointer into list of mapped files */
	off64_t map_offset;	/* offset into core file (if core) */
} map_info_t;

typedef struct lwp_info {	/* per-lwp information from core file */
	list_t	lwp_list;	/* linked list */
	lwpid_t	lwp_id;		/* lwp identifier */
	lwpsinfo_t lwp_psinfo;	/* /proc/<pid>/lwp/<lwpid>/lwpsinfo data */
	lwpstatus_t lwp_status;	/* /proc/<pid>/lwp/<lwpid>/lwpstatus data */
#if defined(sparc) || defined(__sparc)
	gwindows_t *lwp_gwins;	/* /proc/<pid>/lwp/<lwpid>/gwindows data */
	prxregset_t *lwp_xregs;	/* /proc/<pid>/lwp/<lwpid>/xregs data */
	int64_t *lwp_asrs;	/* /proc/<pid>/lwp/<lwpid>/asrs data */
#endif
} lwp_info_t;

typedef struct core_info {	/* information specific to core files */
	char core_dmodel;	/* data model for core file */
	int core_errno;		/* error during initialization if != 0 */
	list_t core_lwp_head;	/* head of list of lwp info */
	lwp_info_t *core_lwp;	/* current lwp information */
	uint_t core_nlwp;	/* number of lwp's in list */
	off64_t core_size;	/* size of core file in bytes */
	char *core_platform;	/* platform string from core file */
	struct utsname *core_uts;	/* uname(2) data from core file */
	prcred_t *core_cred;	/* process credential from core file */
	core_content_t core_content;	/* content dumped to core file */
	prpriv_t *core_priv;	/* process privileges from core file */
	size_t core_priv_size;	/* size of the privileges */
	void *core_privinfo;	/* system privileges info from core file */
	priv_impl_info_t *core_ppii;	/* NOTE entry for core_privinfo */
	char *core_zonename;	/* zone name from core file */
#if defined(__i386) || defined(__amd64)
	struct ssd *core_ldt;	/* LDT entries from core file */
	uint_t core_nldt;	/* number of LDT entries in core file */
#endif
} core_info_t;

typedef struct elf_file {	/* convenience for managing ELF files */
	GElf_Ehdr e_hdr;	/* ELF file header information */
	Elf *e_elf;		/* ELF library handle */
	int e_fd;		/* file descriptor */
} elf_file_t;

typedef struct ps_rwops {	/* ops vector for Pread() and Pwrite() */
	ssize_t (*p_pread)(struct ps_prochandle *,
	    void *, size_t, uintptr_t);
	ssize_t (*p_pwrite)(struct ps_prochandle *,
	    const void *, size_t, uintptr_t);
} ps_rwops_t;

#define	HASHSIZE		1024	/* hash table size, power of 2 */

struct ps_prochandle {
	struct ps_lwphandle **hashtab;	/* hash table for LWPs (Lgrab()) */
	mutex_t	proc_lock;	/* protects hash table; serializes Lgrab() */
	pstatus_t orig_status;	/* remembered status on Pgrab() */
	pstatus_t status;	/* status when stopped */
	psinfo_t psinfo;	/* psinfo_t from last Ppsinfo() request */
	uintptr_t sysaddr;	/* address of most recent syscall instruction */
	pid_t	pid;		/* process-ID */
	int	state;		/* state of the process, see "libproc.h" */
	uint_t	flags;		/* see defines below */
	uint_t	agentcnt;	/* Pcreate_agent()/Pdestroy_agent() ref count */
	int	asfd;		/* /proc/<pid>/as filedescriptor */
	int	ctlfd;		/* /proc/<pid>/ctl filedescriptor */
	int	statfd;		/* /proc/<pid>/status filedescriptor */
	int	agentctlfd;	/* /proc/<pid>/lwp/agent/ctl */
	int	agentstatfd;	/* /proc/<pid>/lwp/agent/status */
	int	info_valid;	/* if zero, map and file info need updating */
	map_info_t *mappings;	/* cached process mappings */
	size_t	map_count;	/* number of mappings */
	uint_t	num_files;	/* number of file elements in file_info */
	list_t	file_head;	/* head of mapped files w/ symbol table info */
	char	*execname;	/* name of the executable file */
	auxv_t	*auxv;		/* the process's aux vector */
	int	nauxv;		/* number of aux vector entries */
	rd_agent_t *rap;	/* cookie for rtld_db */
	map_info_t *map_exec;	/* the mapping for the executable file */
	map_info_t *map_ldso;	/* the mapping for ld.so.1 */
	const ps_rwops_t *ops;	/* pointer to ops-vector for read and write */
	core_info_t *core;	/* information specific to core (if PS_DEAD) */
	uintptr_t *ucaddrs;	/* ucontext-list addresses */
	uint_t	ucnelems;	/* number of elements in the ucaddrs list */
};

/* flags */
#define	CREATED		0x01	/* process was created by Pcreate() */
#define	SETSIG		0x02	/* set signal trace mask before continuing */
#define	SETFAULT	0x04	/* set fault trace mask before continuing */
#define	SETENTRY	0x08	/* set sysentry trace mask before continuing */
#define	SETEXIT		0x10	/* set sysexit trace mask before continuing */
#define	SETHOLD		0x20	/* set signal hold mask before continuing */
#define	SETREGS		0x40	/* set registers before continuing */

struct ps_lwphandle {
	struct ps_prochandle *lwp_proc;	/* process to which this lwp belongs */
	struct ps_lwphandle *lwp_hash;	/* hash table linked list */
	lwpstatus_t	lwp_status;	/* status when stopped */
	lwpsinfo_t	lwp_psinfo;	/* lwpsinfo_t from last Lpsinfo() */
	lwpid_t		lwp_id;		/* lwp identifier */
	int		lwp_state;	/* state of the lwp, see "libproc.h" */
	uint_t		lwp_flags;	/* SETHOLD and/or SETREGS */
	int		lwp_ctlfd;	/* /proc/<pid>/lwp/<lwpid>/lwpctl */
	int		lwp_statfd;	/* /proc/<pid>/lwp/<lwpid>/lwpstatus */
};

/*
 * Implementation functions in the process control library.
 * These are not exported to clients of the library.
 */
extern	void	prldump(const char *, lwpstatus_t *);
extern	int	dupfd(int, int);
extern	int	set_minfd(void);
extern	int	Pscantext(struct ps_prochandle *);
extern	void	Pinitsym(struct ps_prochandle *);
extern	void	Preadauxvec(struct ps_prochandle *);
extern	void	optimize_symtab(sym_tbl_t *);
extern	void	Pbuild_file_symtab(struct ps_prochandle *, file_info_t *);
extern	ctf_file_t *Pbuild_file_ctf(struct ps_prochandle *, file_info_t *);
extern	map_info_t *Paddr2mptr(struct ps_prochandle *, uintptr_t);
extern	char 	*Pfindexec(struct ps_prochandle *, const char *,
	int (*)(const char *, void *), void *);
extern	int	getlwpstatus(struct ps_prochandle *, lwpid_t, lwpstatus_t *);
int	Pstopstatus(struct ps_prochandle *, long, uint32_t);

/*
 * Architecture-dependent definition of the breakpoint instruction.
 */
#if defined(sparc) || defined(__sparc)
#define	BPT	((instr_t)0x91d02001)
#elif defined(__i386) || defined(__amd64)
#define	BPT	((instr_t)0xcc)
#endif

/*
 * Simple convenience.
 */
#define	TRUE	1
#define	FALSE	0

#ifdef	__cplusplus
}
#endif

#endif	/* _PCONTROL_H */
