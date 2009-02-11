# if !defined(SYS_PROCFS_H)
# define	SYS_PROCFS_H

# include	<linux_types.h>
# include	<sys/procfs_isa.h>
# include	<sys/processor.h>
# include	<sys/signal.h>
# include	<sys/priv.h>
# include	<sys/fault.h>

/*
 * System call interfaces for /proc.
 */

/*
 * Control codes (long values) for messages written to ctl and lwpctl files.
 */
#define PCNULL   0L     /* null request, advance to next message */
#define PCSTOP   1L     /* direct process or lwp to stop and wait for stop */
#define PCDSTOP  2L     /* direct process or lwp to stop */
#define PCWSTOP  3L     /* wait for process or lwp to stop, no timeout */
#define PCTWSTOP 4L     /* wait for stop, with long millisecond timeout arg */
#define PCRUN    5L     /* make process/lwp runnable, w/ long flags argument */
#define PCCSIG   6L     /* clear current signal from lwp */
#define PCCFAULT 7L     /* clear current fault from lwp */
#define PCSSIG   8L     /* set current signal from siginfo_t argument */
#define PCKILL   9L     /* post a signal to process/lwp, long argument */
#define PCUNKILL 10L    /* delete a pending signal from process/lwp, long arg */
#define PCSHOLD  11L    /* set lwp signal mask from sigset_t argument */
#define PCSTRACE 12L    /* set traced signal set from sigset_t argument */
#define PCSFAULT 13L    /* set traced fault set from fltset_t argument */
#define PCSENTRY 14L    /* set traced syscall entry set from sysset_t arg */
#define PCSEXIT  15L    /* set traced syscall exit set from sysset_t arg */
#define PCSET    16L    /* set modes from long argument */
#define PCUNSET  17L    /* unset modes from long argument */
#define PCSREG   18L    /* set lwp general registers from prgregset_t arg */
#define PCSFPREG 19L    /* set lwp floating-point registers from prfpregset_t */
#define PCSXREG  20L    /* set lwp extra registers from prxregset_t arg */
#define PCNICE   21L    /* set nice priority from long argument */
#define PCSVADDR 22L    /* set %pc virtual address from long argument */
#define PCWATCH  23L    /* set/unset watched memory area from prwatch_t arg */
#define PCAGENT  24L    /* create agent lwp with regs from prgregset_t arg */
#define PCREAD   25L    /* read from the address space via priovec_t arg */
#define PCWRITE  26L    /* write to the address space via priovec_t arg */
#define PCSCRED  27L    /* set process credentials from prcred_t argument */
#define PCSASRS  28L    /* set ancillary state registers from asrset_t arg */
#define PCSPRIV  29L    /* set process privileges from prpriv_t argument */
#define PCSZONE  30L    /* set zoneid from zoneid_t argument */
#define PCSCREDX 31L    /* as PCSCRED but with supplemental groups */
/*
 * PCRUN long operand flags.
 */
#define PRCSIG          0x01    /* clear current signal, if any */
#define PRCFAULT        0x02    /* clear current fault, if any */
#define PRSTEP          0x04    /* direct the lwp to single-step */
#define PRSABORT        0x08    /* abort syscall, if in syscall */
#define PRSTOP          0x10    /* set directed stop request */


/* Protection and attribute flags */
#define MA_READ         0x04    /* readable by the traced process */
#define MA_WRITE        0x02    /* writable by the traced process */
#define MA_EXEC         0x01    /* executable by the traced process */
#define MA_SHARED       0x08    /* changes are shared by mapped object */
#define MA_ANON         0x40    /* anonymous memory (e.g. /dev/zero) */
#define MA_ISM          0x80    /* intimate shared mem (shared MMU resources) */
#define MA_NORESERVE    0x100   /* mapped with MAP_NORESERVE */
#define MA_SHM          0x200   /* System V shared memory */
#define MA_RESERVED1    0x400   /* reserved for future use */

/*
 * These are obsolete and unreliable.
 * They are included here only for historical compatibility.
 */
#define MA_BREAK        0x10    /* grown by brk(2) */
#define MA_STACK        0x20    /* grown automatically on stack faults */


/*
 * pr_flags (same values appear in both pstatus_t and lwpstatus_t pr_flags).
 *
 * These flags do *not* apply to psinfo_t.pr_flag or lwpsinfo_t.pr_flag
 * (which are both deprecated).
 */
/* The following flags apply to the specific or representative lwp */
#define PR_STOPPED 0x00000001   /* lwp is stopped */
#define PR_ISTOP   0x00000002   /* lwp is stopped on an event of interest */
#define PR_DSTOP   0x00000004   /* lwp has a stop directive in effect */
#define PR_STEP    0x00000008   /* lwp has a single-step directive in effect */
#define PR_ASLEEP  0x00000010   /* lwp is sleeping in a system call */
#define PR_PCINVAL 0x00000020   /* contents of pr_instr undefined */
#define PR_ASLWP   0x00000040   /* obsolete flag; never set */
#define PR_AGENT   0x00000080   /* this lwp is the /proc agent lwp */
#define PR_DETACH  0x00000100   /* this is a detached lwp */
#define PR_DAEMON  0x00000200   /* this is a daemon lwp */
/* The following flags apply to the process, not to an individual lwp */
#define PR_ISSYS   0x00001000   /* this is a system process */
#define PR_VFORKP  0x00002000   /* process is the parent of a vfork()d child */
#define PR_ORPHAN  0x00004000   /* process's process group is orphaned */
/* The following process flags are modes settable by PCSET/PCUNSET */
#define PR_FORK    0x00100000   /* inherit-on-fork is in effect */
#define PR_RLC     0x00200000   /* run-on-last-close is in effect */
#define PR_KLC     0x00400000   /* kill-on-last-close is in effect */
#define PR_ASYNC   0x00800000   /* asynchronous-stop is in effect */
#define PR_MSACCT  0x01000000   /* micro-state usage accounting is in effect */
#define PR_BPTADJ  0x02000000   /* breakpoint trap pc adjustment is in effect */
#define PR_PTRACE  0x04000000   /* ptrace-compatibility mode is in effect */
#define PR_MSFORK  0x08000000   /* micro-state accounting inherited on fork */
#define PR_IDLE    0x10000000   /* lwp is a cpu's idle thread */

/*
 * See <sys/procfs_isa.h> for possible values of pr_dmodel.
 */

/*
 * Reasons for stopping (pr_why).
 */
#define PR_REQUESTED    1
#define PR_SIGNALLED    2
#define PR_SYSENTRY     3
#define PR_SYSEXIT      4
#define PR_JOBCONTROL   5
#define PR_FAULTED      6
#define PR_SUSPENDED    7
#define PR_CHECKPOINT   8

typedef struct {                /* syscall set type */
        unsigned int    word[16];
} sysset_t;

typedef int psetid_t;
typedef id_t    taskid_t;
typedef id_t    poolid_t;               /* pool ID type         */
typedef id_t    projid_t;
typedef uint32_t priv_chunk_t;

struct fpq32 {
        caddr32_t       fpq_addr;       /* address */
        uint32_t        fpq_instr;      /* instruction */
};

struct fq32 {
        union {                         /* FPU inst/addr queue */
                double whole;
                struct fpq32 fpq;
        } FQu;
};

typedef struct prfpregset32 {
        union {                         /* FPU floating point regs */
                uint32_t pr_regs[32];           /* 32 singles */
                double  pr_dregs[16];           /* 16 doubles */
        } pr_fr;
        uint32_t pr_filler;
        uint32_t pr_fsr;                /* FPU status register */
        uint8_t pr_qcnt;                /* # of entries in saved FQ */
        uint8_t pr_q_entrysize;         /* # of bytes per FQ entry */
        uint8_t pr_en;                  /* flag signifying fpu in use */
        struct fq32 pr_q[32];           /* contains the FQ array */
} prfpregset32_t;

/*
 * Memory-map interface.  /proc/<pid>/map /proc/<pid>/rmap
 */
#define PRMAPSZ 64
typedef struct prmap {
        uintptr_t pr_vaddr;     /* virtual address of mapping */
        size_t  pr_size;        /* size of mapping in bytes */
        char    pr_mapname[PRMAPSZ];    /* name in /proc/<pid>/object */
        offset_t pr_offset;     /* offset into mapped object, if any */
        int     pr_mflags;      /* protection and attribute flags (see below) */
        int     pr_pagesize;    /* pagesize (bytes) for this mapping */
        int     pr_shmid;       /* SysV shmid, -1 if not SysV shared memory */
        int     pr_filler[1];   /* filler for future expansion */
} prmap_t;


#define	PRCLSZ		8	/* maximum size of scheduling class name */
#define	PRSYSARGS	8	/* maximum number of syscall arguments */
/*
 * _ILP32 lwp status file.  /proc/<pid>/lwp/<lwpid>/lwpstatus
 */
typedef struct lwpstatus32 {
        int     pr_flags;       /* flags */
        id32_t  pr_lwpid;       /* specific lwp identifier */
        short   pr_why;         /* reason for lwp stop, if stopped */
        short   pr_what;        /* more detailed reason */
        short   pr_cursig;      /* current signal, if any */
        short   pr_pad1;
        siginfo32_t pr_info;    /* info associated with signal or fault */
        sigset_t pr_lwppend;    /* set of signals pending to the lwp */
        sigset_t pr_lwphold;    /* set of signals blocked by the lwp */
        struct sigaction32 pr_action;   /* signal action for current signal */
        stack32_t pr_altstack;  /* alternate signal stack info */
        caddr32_t pr_oldcontext;        /* address of previous ucontext */
        short   pr_syscall;     /* system call number (if in syscall) */
        short   pr_nsysarg;     /* number of arguments to this syscall */
        int     pr_errno;       /* errno for failed syscall, 0 if successful */
        int32_t pr_sysarg[PRSYSARGS];   /* arguments to this syscall */
        int32_t pr_rval1;       /* primary syscall return value */
        int32_t pr_rval2;       /* second syscall return value, if any */
        char    pr_clname[PRCLSZ];      /* scheduling class name */
        timestruc32_t pr_tstamp;        /* real-time time stamp of stop */
        timestruc32_t pr_utime; /* lwp user cpu time */
        timestruc32_t pr_stime; /* lwp system cpu time */
        int     pr_filler[11 - 2 * sizeof (timestruc32_t) / sizeof (int)];
        int     pr_errpriv;     /* missing privilege */
        caddr32_t pr_ustack;    /* address of stack boundary data (stack32_t) */
        uint32_t pr_instr;      /* current instruction */
        prgregset32_t pr_reg;   /* general registers */
        prfpregset32_t pr_fpreg; /* floating-point registers */
} lwpstatus32_t;

typedef struct lwpstatus {
	int	pr_flags;	/* flags (see below) */
	id_t	pr_lwpid;	/* specific lwp identifier */
	short	pr_why;		/* reason for lwp stop, if stopped */
	short	pr_what;	/* more detailed reason */
	short	pr_cursig;	/* current signal, if any */
	short	pr_pad1;
	siginfo_t pr_info;	/* info associated with signal or fault */
	sigset_t pr_lwppend;	/* set of signals pending to the lwp */
	sigset_t pr_lwphold;	/* set of signals blocked by the lwp */
	struct sigaction pr_action;	/* signal action for current signal */
	stack_t	pr_altstack;	/* alternate signal stack info */
	uintptr_t pr_oldcontext;	/* address of previous ucontext */
	short	pr_syscall;	/* system call number (if in syscall) */
	short	pr_nsysarg;	/* number of arguments to this syscall */
	int	pr_errno;	/* errno for failed syscall, 0 if successful */
	long	pr_sysarg[PRSYSARGS];	/* arguments to this syscall */
	long	pr_rval1;	/* primary syscall return value */
	long	pr_rval2;	/* second syscall return value, if any */
	char	pr_clname[PRCLSZ];	/* scheduling class name */
	timestruc_t pr_tstamp;	/* real-time time stamp of stop */
	timestruc_t pr_utime;	/* lwp user cpu time */
	timestruc_t pr_stime;	/* lwp system cpu time */
	int	pr_filler[11 - 2 * sizeof (timestruc_t) / sizeof (int)];
	int	pr_errpriv;	/* missing privilege */
	uintptr_t pr_ustack;	/* address of stack boundary data (stack_t) */
	ulong_t	pr_instr;	/* current instruction */
	prgregset_t pr_reg;	/* general registers */
	prfpregset_t pr_fpreg;	/* floating-point registers */
} lwpstatus_t;

/*
 * _ILP32 process status file.  /proc/<pid>/status
 */
typedef struct pstatus32 {
        int     pr_flags;       /* flags */
        int     pr_nlwp;        /* number of active lwps in the process */
        pid32_t pr_pid;         /* process id */
        pid32_t pr_ppid;        /* parent process id */
        pid32_t pr_pgid;        /* process group id */
        pid32_t pr_sid;         /* session id */
        id32_t  pr_aslwpid;     /* historical; now always zero */
        id32_t  pr_agentid;     /* lwp id of the /proc agent lwp, if any */
        sigset_t pr_sigpend;    /* set of process pending signals */
        caddr32_t pr_brkbase;   /* address of the process heap */
        size32_t pr_brksize;    /* size of the process heap, in bytes */
        caddr32_t pr_stkbase;   /* address of the process stack */
        size32_t pr_stksize;    /* size of the process stack, in bytes */
        timestruc32_t pr_utime; /* process user cpu time */
        timestruc32_t pr_stime; /* process system cpu time */
        timestruc32_t pr_cutime;        /* sum of children's user times */
        timestruc32_t pr_cstime;        /* sum of children's system times */
        sigset_t pr_sigtrace;   /* set of traced signals */
        fltset_t pr_flttrace;   /* set of traced faults */
        sysset_t pr_sysentry;   /* set of system calls traced on entry */
        sysset_t pr_sysexit;    /* set of system calls traced on exit */
        char    pr_dmodel;      /* data model of the process */
        char    pr_pad[3];
        id32_t  pr_taskid;      /* task id */
        id32_t  pr_projid;      /* project id */
        int     pr_nzomb;       /* number of zombie lwps in the process */
        id32_t  pr_zoneid;      /* zone id */
        int     pr_filler[15];  /* reserved for future use */
        lwpstatus32_t pr_lwp;   /* status of the representative lwp */
} pstatus32_t;

typedef struct pstatus {
        int     pr_flags;       /* flags (see below) */
        int     pr_nlwp;        /* number of active lwps in the process */
        pid_t   pr_pid;         /* process id */
        pid_t   pr_ppid;        /* parent process id */
        pid_t   pr_pgid;        /* process group id */
        pid_t   pr_sid;         /* session id */
        id_t    pr_aslwpid;     /* historical; now always zero */
        id_t    pr_agentid;     /* lwp id of the /proc agent lwp, if any */
        sigset_t pr_sigpend;    /* set of process pending signals */
        uintptr_t pr_brkbase;   /* address of the process heap */
        size_t  pr_brksize;     /* size of the process heap, in bytes */
        uintptr_t pr_stkbase;   /* address of the process stack */
        size_t  pr_stksize;     /* size of the process stack, in bytes */
        timestruc_t pr_utime;   /* process user cpu time */
        timestruc_t pr_stime;   /* process system cpu time */
        timestruc_t pr_cutime;  /* sum of children's user times */
        timestruc_t pr_cstime;  /* sum of children's system times */
        sigset_t pr_sigtrace;   /* set of traced signals */
        fltset_t pr_flttrace;   /* set of traced faults */
        sysset_t pr_sysentry;   /* set of system calls traced on entry */
        sysset_t pr_sysexit;    /* set of system calls traced on exit */
        char    pr_dmodel;      /* data model of the process (see below) */
        char    pr_pad[3];
        taskid_t pr_taskid;     /* task id */
        projid_t pr_projid;     /* project id */
        int     pr_nzomb;       /* number of zombie lwps in the process */
        zoneid_t pr_zoneid;     /* zone id */
        int     pr_filler[15];  /* reserved for future use */
        lwpstatus_t pr_lwp;     /* status of the representative lwp */
} pstatus_t;


#define PRFNSZ          16      /* Maximum size of execed filename */

/*
 * _ILP32 lwp ps(1) information file.  /proc/<pid>/lwp/<lwpid>/lwpsinfo
 */
typedef struct lwpsinfo32 {
        int     pr_flag;        /* lwp flags */
        id32_t  pr_lwpid;       /* lwp id */
        caddr32_t pr_addr;      /* internal address of lwp */
        caddr32_t pr_wchan;     /* wait addr for sleeping lwp */
        char    pr_stype;       /* synchronization event type */
        char    pr_state;       /* numeric lwp state */
        char    pr_sname;       /* printable character for pr_state */
        char    pr_nice;        /* nice for cpu usage */
        short   pr_syscall;     /* system call number (if in syscall) */
        char    pr_oldpri;      /* pre-SVR4, low value is high priority */
        char    pr_cpu;         /* pre-SVR4, cpu usage for scheduling */
        int     pr_pri;         /* priority, high value is high priority */
                        /* The following percent number is a 16-bit binary */
                        /* fraction [0 .. 1] with the binary point to the */
                        /* right of the high-order bit (1.0 == 0x8000) */
        ushort_t pr_pctcpu;     /* % of recent cpu time used by this lwp */
        ushort_t pr_pad;
        timestruc32_t pr_start; /* lwp start time, from the epoch */
        timestruc32_t pr_time;  /* usr+sys cpu time for this lwp */
        char    pr_clname[PRCLSZ];      /* scheduling class name */
        char    pr_name[PRFNSZ];        /* name of system lwp */
        processorid_t pr_onpro;         /* processor which last ran this lwp */
        processorid_t pr_bindpro;       /* processor to which lwp is bound */
        psetid_t pr_bindpset;   /* processor set to which lwp is bound */
        int     pr_filler[5];   /* reserved for future use */
} lwpsinfo32_t;


typedef struct lwpsinfo {
        int     pr_flag;        /* lwp flags (DEPRECATED; do not use) */
        id_t    pr_lwpid;       /* lwp id */
        uintptr_t pr_addr;      /* internal address of lwp */
        uintptr_t pr_wchan;     /* wait addr for sleeping lwp */
        char    pr_stype;       /* synchronization event type */
        char    pr_state;       /* numeric lwp state */
        char    pr_sname;       /* printable character for pr_state */
        char    pr_nice;        /* nice for cpu usage */
        short   pr_syscall;     /* system call number (if in syscall) */
        char    pr_oldpri;      /* pre-SVR4, low value is high priority */
        char    pr_cpu;         /* pre-SVR4, cpu usage for scheduling */
        int     pr_pri;         /* priority, high value is high priority */
                        /* The following percent number is a 16-bit binary */
                        /* fraction [0 .. 1] with the binary point to the */
                        /* right of the high-order bit (1.0 == 0x8000) */
        ushort_t pr_pctcpu;     /* % of recent cpu time used by this lwp */
        ushort_t pr_pad;
        timestruc_t pr_start;   /* lwp start time, from the epoch */
        timestruc_t pr_time;    /* usr+sys cpu time for this lwp */
        char    pr_clname[PRCLSZ];      /* scheduling class name */
        char    pr_name[PRFNSZ];        /* name of system lwp */
        processorid_t pr_onpro;         /* processor which last ran this lwp */
        processorid_t pr_bindpro;       /* processor to which lwp is bound */
        psetid_t pr_bindpset;   /* processor set to which lwp is bound */
        int     pr_filler[5];   /* reserved for future use */
} lwpsinfo_t;

/*
 * _ILP32 process ps(1) information file.  /proc/<pid>/psinfo
 */
#define PRARGSZ         80      /* number of chars of arguments */
typedef struct psinfo32 {
        int     pr_flag;        /* process flags */
        int     pr_nlwp;        /* number of active lwps in the process */
        pid32_t pr_pid;         /* unique process id */
        pid32_t pr_ppid;        /* process id of parent */
        pid32_t pr_pgid;        /* pid of process group leader */
        pid32_t pr_sid;         /* session id */
        uid32_t pr_uid;         /* real user id */
        uid32_t pr_euid;        /* effective user id */
        gid32_t pr_gid;         /* real group id */
        gid32_t pr_egid;        /* effective group id */
        caddr32_t pr_addr;      /* address of process */
        size32_t pr_size;       /* size of process image in Kbytes */
        size32_t pr_rssize;     /* resident set size in Kbytes */
        size32_t pr_pad1;
        dev32_t pr_ttydev;      /* controlling tty device (or PRNODEV) */
        ushort_t pr_pctcpu;     /* % of recent cpu time used by all lwps */
        ushort_t pr_pctmem;     /* % of system memory used by process */
        timestruc32_t pr_start; /* process start time, from the epoch */
        timestruc32_t pr_time;  /* usr+sys cpu time for this process */
        timestruc32_t pr_ctime; /* usr+sys cpu time for reaped children */
        char    pr_fname[PRFNSZ];       /* name of execed file */
        char    pr_psargs[PRARGSZ];     /* initial characters of arg list */
        int     pr_wstat;       /* if zombie, the wait() status */
        int     pr_argc;        /* initial argument count */
        caddr32_t pr_argv;      /* address of initial argument vector */
        caddr32_t pr_envp;      /* address of initial environment vector */
        char    pr_dmodel;      /* data model of the process */
        char    pr_pad2[3];
        id32_t  pr_taskid;      /* task id */
        id32_t  pr_projid;      /* project id */
        int     pr_nzomb;       /* number of zombie lwps in the process */
        id32_t  pr_poolid;      /* pool id */
        id32_t  pr_zoneid;      /* zone id */
        id32_t  pr_contract;    /* process contract */
        int     pr_filler[1];   /* reserved for future use */
        lwpsinfo32_t pr_lwp;    /* information for representative lwp */
} psinfo32_t;

/*
 * process ps(1) information file.  /proc/<pid>/psinfo
 */
typedef struct psinfo {
        int     pr_flag;        /* process flags (DEPRECATED; do not use) */
        int     pr_nlwp;        /* number of active lwps in the process */
        pid_t   pr_pid;         /* unique process id */
        pid_t   pr_ppid;        /* process id of parent */
        pid_t   pr_pgid;        /* pid of process group leader */
        pid_t   pr_sid;         /* session id */
        uid_t   pr_uid;         /* real user id */
        uid_t   pr_euid;        /* effective user id */
        gid_t   pr_gid;         /* real group id */
        gid_t   pr_egid;        /* effective group id */
        uintptr_t pr_addr;      /* address of process */
        size_t  pr_size;        /* size of process image in Kbytes */
        size_t  pr_rssize;      /* resident set size in Kbytes */
        size_t  pr_pad1;
        dev_t   pr_ttydev;      /* controlling tty device (or PRNODEV) */
                        /* The following percent numbers are 16-bit binary */
                        /* fractions [0 .. 1] with the binary point to the */
                        /* right of the high-order bit (1.0 == 0x8000) */
        ushort_t pr_pctcpu;     /* % of recent cpu time used by all lwps */
        ushort_t pr_pctmem;     /* % of system memory used by process */
        timestruc_t pr_start;   /* process start time, from the epoch */
        timestruc_t pr_time;    /* usr+sys cpu time for this process */
        timestruc_t pr_ctime;   /* usr+sys cpu time for reaped children */
        char    pr_fname[PRFNSZ];       /* name of execed file */
        char    pr_psargs[PRARGSZ];     /* initial characters of arg list */
        int     pr_wstat;       /* if zombie, the wait() status */
        int     pr_argc;        /* initial argument count */
        uintptr_t pr_argv;      /* address of initial argument vector */
        uintptr_t pr_envp;      /* address of initial environment vector */
        char    pr_dmodel;      /* data model of the process */
        char    pr_pad2[3];
        taskid_t pr_taskid;     /* task id */
        projid_t pr_projid;     /* project id */
        int     pr_nzomb;       /* number of zombie lwps in the process */
        poolid_t pr_poolid;     /* pool id */
        zoneid_t pr_zoneid;     /* zone id */
        id_t    pr_contract;    /* process contract */
        int     pr_filler[1];   /* reserved for future use */
        lwpsinfo_t pr_lwp;      /* information for representative lwp */
} psinfo_t;

/*
 * Process credentials.  PCSCRED and /proc/<pid>/cred
 */
typedef struct prcred {
        uid_t   pr_euid;        /* effective user id */
        uid_t   pr_ruid;        /* real user id */
        uid_t   pr_suid;        /* saved user id (from exec) */
        gid_t   pr_egid;        /* effective group id */
        gid_t   pr_rgid;        /* real group id */
        gid_t   pr_sgid;        /* saved group id (from exec) */
        int     pr_ngroups;     /* number of supplementary groups */
        gid_t   pr_groups[1];   /* array of supplementary groups */
} prcred_t;

/*
 * Process privileges.  PCSPRIV and /proc/<pid>/priv
 */
typedef struct prpriv {
        uint32_t        pr_nsets;               /* number of privilege set */
        uint32_t        pr_setsize;             /* size of privilege set */
        uint32_t        pr_infosize;            /* size of supplementary data */
        priv_chunk_t    pr_sets[1];             /* array of sets */
} prpriv_t;

/*
 * Watchpoint interface.  PCWATCH and /proc/<pid>/watch
 */
typedef struct prwatch {
        uintptr_t pr_vaddr;     /* virtual address of watched area */
        size_t  pr_size;        /* size of watched area in bytes */
        int     pr_wflags;      /* watch type flags */
        int     pr_pad;
} prwatch_t;

typedef struct {        /* return values from system call */
        long    sys_rval1;      /* primary return value from system call */
        long    sys_rval2;      /* second return value from system call */
} sysret_t;

#define PRNODEV (dev_t)(-1)     /* non-existent device */
#define PRNODEV32 (dev32_t)(-1)

/*
 * Header for /proc/<pid>/lstatus /proc/<pid>/lpsinfo /proc/<pid>/lusage
 */
typedef struct prheader {
        long    pr_nent;        /* number of entries */
        long    pr_entsize;     /* size of each entry, in bytes */
} prheader_t;

/*
 * PCREAD/PCWRITE I/O interface.
 */
typedef struct priovec {
        void    *pio_base;      /* buffer in controlling process */
        size_t  pio_len;        /* size of read/write request */
        off_t   pio_offset;     /* virtual address in target process */
} priovec_t;
/* turn on all flags in set */
#define prfillset(sp) \
        { register int _i_ = sizeof (*(sp))/sizeof (uint32_t); \
                while (_i_) ((uint32_t *)(sp))[--_i_] = (uint32_t)0xFFFFFFFF; }

/* turn off all flags in set */
#define premptyset(sp) \
        { register int _i_ = sizeof (*(sp))/sizeof (uint32_t); \
                while (_i_) ((uint32_t *)(sp))[--_i_] = (uint32_t)0; }

/* turn on specified flag in set */
#define praddset(sp, flag) \
        ((void)(((unsigned)((flag)-1) < 32*sizeof (*(sp))/sizeof (uint32_t)) ? \
        (((uint32_t *)(sp))[((flag)-1)/32] |= (1U<<(((flag)-1)%32))) : 0))

/* turn off specified flag in set */
#define prdelset(sp, flag) \
        ((void)(((unsigned)((flag)-1) < 32*sizeof (*(sp))/sizeof (uint32_t)) ? \
            (((uint32_t *)(sp))[((flag)-1)/32] &= ~(1U<<(((flag)-1)%32))) : 0))

/* query: != 0 iff flag is turned on in set */
#define prismember(sp, flag) \
        (((unsigned)((flag)-1) < 32*sizeof (*(sp))/sizeof (uint32_t)) && \
            (((uint32_t *)(sp))[((flag)-1)/32] & (1U<<(((flag)-1)%32))))
# endif
