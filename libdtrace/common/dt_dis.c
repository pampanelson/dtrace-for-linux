/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)dt_dis.c	1.4	04/12/17 SMI"

#include <linux_types.h>
#include <strings.h>
#include <stdio.h>
#include <dt_ident.h>
#include <dt_impl.h>

/*ARGSUSED*/
static void
dt_dis_log(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %%r%u, %%r%u, %%r%u", name,
	    DIF_INSTR_R1(in), DIF_INSTR_R2(in), DIF_INSTR_RD(in));
}

/*ARGSUSED*/
static void
dt_dis_branch(const dtrace_difo_t *dp, const char *name,
	dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %u", name, DIF_INSTR_LABEL(in));
}

/*ARGSUSED*/
static void
dt_dis_load(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s [%%r%u], %%r%u", name,
	    DIF_INSTR_R1(in), DIF_INSTR_RD(in));
}

/*ARGSUSED*/
static void
dt_dis_store(const dtrace_difo_t *dp, const char *name,
	dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %%r%u, [%%r%u]", name,
	    DIF_INSTR_R1(in), DIF_INSTR_RD(in));
}

/*ARGSUSED*/
static void
dt_dis_str(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%s", name);
}

/*ARGSUSED*/
static void
dt_dis_r1rd(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %%r%u, %%r%u", name,
	    DIF_INSTR_R1(in), DIF_INSTR_RD(in));
}

/*ARGSUSED*/
static void
dt_dis_cmp(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %%r%u, %%r%u", name,
	    DIF_INSTR_R1(in), DIF_INSTR_R2(in));
}

/*ARGSUSED*/
static void
dt_dis_tst(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %%r%u", name, DIF_INSTR_R1(in));
}

static const char *
dt_dis_varname(const dtrace_difo_t *dp, uint_t id, uint_t scope)
{
	const dtrace_difv_t *dvp = dp->dtdo_vartab;
	uint_t i;

	for (i = 0; i < dp->dtdo_varlen; i++, dvp++) {
		if (dvp->dtdv_id == id && dvp->dtdv_scope == scope) {
			if (dvp->dtdv_name < dp->dtdo_strlen)
				return (dp->dtdo_strtab + dvp->dtdv_name);
			break;
		}
	}

	return (NULL);
}

static uint_t
dt_dis_scope(const char *name)
{
	switch (name[2]) {
	case 'l': return (DIFV_SCOPE_LOCAL);
	case 't': return (DIFV_SCOPE_THREAD);
	case 'g': return (DIFV_SCOPE_GLOBAL);
	default: return (-1u);
	}
}

static void
dt_dis_lda(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	uint_t var = DIF_INSTR_R1(in);
	const char *vname;

	(void) fprintf(fp, "%-4s DT_VAR(%u), %%r%u, %%r%u", name,
	    var, DIF_INSTR_R2(in), DIF_INSTR_RD(in));

	if ((vname = dt_dis_varname(dp, var, dt_dis_scope(name))) != NULL)
		(void) fprintf(fp, "\t\t! DT_VAR(%u) = \"%s\"", var, vname);
}

static void
dt_dis_ldv(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	uint_t var = DIF_INSTR_VAR(in);
	const char *vname;

	(void) fprintf(fp, "%-4s DT_VAR(%u), %%r%u",
	    name, var, DIF_INSTR_RD(in));

	if ((vname = dt_dis_varname(dp, var, dt_dis_scope(name))) != NULL)
		(void) fprintf(fp, "\t\t! DT_VAR(%u) = \"%s\"", var, vname);
}

static void
dt_dis_stv(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	uint_t var = DIF_INSTR_VAR(in);
	const char *vname;

	(void) fprintf(fp, "%-4s %%r%u, DT_VAR(%u)",
	    name, DIF_INSTR_RS(in), var);

	if ((vname = dt_dis_varname(dp, var, dt_dis_scope(name))) != NULL)
		(void) fprintf(fp, "\t\t! DT_VAR(%u) = \"%s\"", var, vname);
}

static void
dt_dis_setx(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	uint_t intptr = DIF_INSTR_INTEGER(in);

	(void) fprintf(fp, "%-4s DT_INTEGER[%u], %%r%u", name,
	    intptr, DIF_INSTR_RD(in));

	if (intptr < dp->dtdo_intlen) {
		(void) fprintf(fp, "\t\t! 0x%llx",
		    (u_longlong_t)dp->dtdo_inttab[intptr]);
	}
}

static void
dt_dis_sets(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	uint_t strptr = DIF_INSTR_STRING(in);

	(void) fprintf(fp, "%-4s DT_STRING[%u], %%r%u", name,
	    strptr, DIF_INSTR_RD(in));

	if (strptr < dp->dtdo_strlen)
		(void) fprintf(fp, "\t\t! \"%s\"", dp->dtdo_strtab + strptr);
}

/*ARGSUSED*/
static void
dt_dis_ret(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	(void) fprintf(fp, "%-4s %%r%u", name, DIF_INSTR_RD(in));
}

/*ARGSUSED*/
static void
dt_dis_call(const dtrace_difo_t *dp, const char *name, dif_instr_t in, FILE *fp)
{
	static const struct {
		const char *name;
		int subr;
	} snames[] = {
		{ "rand",			DIF_SUBR_RAND },
		{ "mutex_owned",		DIF_SUBR_MUTEX_OWNED },
		{ "mutex_owner",		DIF_SUBR_MUTEX_OWNER },
		{ "mutex_type_adaptive",	DIF_SUBR_MUTEX_TYPE_ADAPTIVE },
		{ "mutex_type_spin",		DIF_SUBR_MUTEX_TYPE_SPIN },
		{ "rw_read_held",		DIF_SUBR_RW_READ_HELD },
		{ "rw_write_held",		DIF_SUBR_RW_WRITE_HELD },
		{ "rw_iswriter",		DIF_SUBR_RW_ISWRITER },
		{ "copyin",			DIF_SUBR_COPYIN },
		{ "copyinstr",			DIF_SUBR_COPYINSTR },
		{ "speculation",		DIF_SUBR_SPECULATION },
		{ "progenyof",			DIF_SUBR_PROGENYOF },
		{ "strlen",			DIF_SUBR_STRLEN },
		{ "copyout",			DIF_SUBR_COPYOUT },
		{ "copyoutstr",			DIF_SUBR_COPYOUTSTR },
		{ "alloca",			DIF_SUBR_ALLOCA },
		{ "bcopy",			DIF_SUBR_BCOPY },
		{ "copyinto",			DIF_SUBR_COPYINTO },
		{ "msgdsize",			DIF_SUBR_MSGDSIZE },
		{ "msgsize",			DIF_SUBR_MSGSIZE },
		{ NULL, 0 }
	};

	uint_t subr = DIF_INSTR_SUBR(in), i;

	(void) fprintf(fp, "%-4s DIF_SUBR(%u), %%r%u",
	    name, subr, DIF_INSTR_RD(in));

	for (i = 0; snames[i].name != NULL; i++) {
		if (subr == snames[i].subr) {
			(void) fprintf(fp, "\t\t! %s", snames[i].name);
			return;
		}
	}
}

/*ARGSUSED*/
static void
dt_dis_pushts(const dtrace_difo_t *dp, const char *name,
	dif_instr_t in, FILE *fp)
{
	static const char *const tnames[] = { "D type", "string" };
	uint_t type = DIF_INSTR_TYPE(in);

	(void) fprintf(fp, "%-4s DT_TYPE(%u), %%r%u, %%r%u",
	    name, type, DIF_INSTR_R2(in), DIF_INSTR_RS(in));

	if (type < sizeof (tnames) / sizeof (tnames[0]))
		(void) fprintf(fp, "\t! DT_TYPE(%u) = %s", type, tnames[type]);
}

static char *
dt_dis_typestr(const dtrace_diftype_t *t, char *buf, size_t len)
{
	char kind[16], ckind[16];

	switch (t->dtdt_kind) {
	case DIF_TYPE_CTF:
		(void) strcpy(kind, "D type");
		break;
	case DIF_TYPE_STRING:
		(void) strcpy(kind, "string");
		break;
	default:
		(void) snprintf(kind, sizeof (kind), "0x%x", t->dtdt_kind);
	}

	switch (t->dtdt_ckind) {
	case CTF_K_UNKNOWN:
		(void) strcpy(ckind, "unknown");
		break;
	case CTF_K_INTEGER:
		(void) strcpy(ckind, "integer");
		break;
	case CTF_K_FLOAT:
		(void) strcpy(ckind, "float");
		break;
	case CTF_K_POINTER:
		(void) strcpy(ckind, "pointer");
		break;
	case CTF_K_ARRAY:
		(void) strcpy(ckind, "array");
		break;
	case CTF_K_FUNCTION:
		(void) strcpy(ckind, "function");
		break;
	case CTF_K_STRUCT:
		(void) strcpy(ckind, "struct");
		break;
	case CTF_K_UNION:
		(void) strcpy(ckind, "union");
		break;
	case CTF_K_ENUM:
		(void) strcpy(ckind, "enum");
		break;
	case CTF_K_FORWARD:
		(void) strcpy(ckind, "forward");
		break;
	case CTF_K_TYPEDEF:
		(void) strcpy(ckind, "typedef");
		break;
	case CTF_K_VOLATILE:
		(void) strcpy(ckind, "volatile");
		break;
	case CTF_K_CONST:
		(void) strcpy(ckind, "const");
		break;
	case CTF_K_RESTRICT:
		(void) strcpy(ckind, "restrict");
		break;
	default:
		(void) snprintf(ckind, sizeof (ckind), "0x%x", t->dtdt_ckind);
	}

	if (t->dtdt_flags & DIF_TF_BYREF) {
		(void) snprintf(buf, len, "%s (%s) by ref (size %lu)",
		    kind, ckind, (ulong_t)t->dtdt_size);
	} else {
		(void) snprintf(buf, len, "%s (%s) (size %lu)",
		    kind, ckind, (ulong_t)t->dtdt_size);
	}

	return (buf);
}

static void
dt_dis_rtab(const char *rtag, const dtrace_difo_t *dp, FILE *fp,
    const dof_relodesc_t *rp, uint32_t len)
{
	(void) fprintf(fp, "\n%-4s %-8s %-8s %s\n",
	    rtag, "OFFSET", "DATA", "NAME");

	for (; len != 0; len--, rp++) {
		(void) fprintf(fp, "%-4u %-8llu %-8llu %s\n",
		    rp->dofr_type, (u_longlong_t)rp->dofr_offset,
		    (u_longlong_t)rp->dofr_data,
		    &dp->dtdo_strtab[rp->dofr_name]);
	}
}

void
dtrace_difo_print(const dtrace_difo_t *dp, FILE *fp)
{
	static const struct opent {
		const char *op_name;
		void (*op_func)(const dtrace_difo_t *, const char *,
		    dif_instr_t, FILE *);
	} optab[] = {
		{ "(illegal opcode)", dt_dis_str },
		{ "or", dt_dis_log },		/* DIF_OP_OR */
		{ "xor", dt_dis_log },		/* DIF_OP_XOR */
		{ "and", dt_dis_log },		/* DIF_OP_AND */
		{ "sll", dt_dis_log },		/* DIF_OP_SLL */
		{ "srl", dt_dis_log },		/* DIF_OP_SRL */
		{ "sub", dt_dis_log },		/* DIF_OP_SUB */
		{ "add", dt_dis_log },		/* DIF_OP_ADD */
		{ "mul", dt_dis_log },		/* DIF_OP_MUL */
		{ "sdiv", dt_dis_log },		/* DIF_OP_SDIV */
		{ "udiv", dt_dis_log },		/* DIF_OP_UDIV */
		{ "srem", dt_dis_log },		/* DIF_OP_SREM */
		{ "urem", dt_dis_log },		/* DIF_OP_UREM */
		{ "not", dt_dis_r1rd },		/* DIF_OP_NOT */
		{ "mov", dt_dis_r1rd },		/* DIF_OP_MOV */
		{ "cmp", dt_dis_cmp },		/* DIF_OP_CMP */
		{ "tst", dt_dis_tst },		/* DIF_OP_TST */
		{ "ba", dt_dis_branch },	/* DIF_OP_BA */
		{ "be", dt_dis_branch },	/* DIF_OP_BE */
		{ "bne", dt_dis_branch },	/* DIF_OP_BNE */
		{ "bg", dt_dis_branch },	/* DIF_OP_BG */
		{ "bgu", dt_dis_branch },	/* DIF_OP_BGU */
		{ "bge", dt_dis_branch },	/* DIF_OP_BGE */
		{ "bgeu", dt_dis_branch },	/* DIF_OP_BGEU */
		{ "bl", dt_dis_branch },	/* DIF_OP_BL */
		{ "blu", dt_dis_branch },	/* DIF_OP_BLU */
		{ "ble", dt_dis_branch },	/* DIF_OP_BLE */
		{ "bleu", dt_dis_branch },	/* DIF_OP_BLEU */
		{ "ldsb", dt_dis_load },	/* DIF_OP_LDSB */
		{ "ldsh", dt_dis_load },	/* DIF_OP_LDSH */
		{ "ldsw", dt_dis_load },	/* DIF_OP_LDSW */
		{ "ldub", dt_dis_load },	/* DIF_OP_LDUB */
		{ "lduh", dt_dis_load },	/* DIF_OP_LDUH */
		{ "lduw", dt_dis_load },	/* DIF_OP_LDUW */
		{ "ldx", dt_dis_load },		/* DIF_OP_LDX */
		{ "ret", dt_dis_ret },		/* DIF_OP_RET */
		{ "nop", dt_dis_str },		/* DIF_OP_NOP */
		{ "setx", dt_dis_setx },	/* DIF_OP_SETX */
		{ "sets", dt_dis_sets },	/* DIF_OP_SETS */
		{ "scmp", dt_dis_cmp },		/* DIF_OP_SCMP */
		{ "ldga", dt_dis_lda },		/* DIF_OP_LDGA */
		{ "ldgs", dt_dis_ldv },		/* DIF_OP_LDGS */
		{ "stgs", dt_dis_stv },		/* DIF_OP_STGS */
		{ "ldta", dt_dis_lda },		/* DIF_OP_LDTA */
		{ "ldts", dt_dis_ldv },		/* DIF_OP_LDTS */
		{ "stts", dt_dis_stv },		/* DIF_OP_STTS */
		{ "sra", dt_dis_log },		/* DIF_OP_SRA */
		{ "call", dt_dis_call },	/* DIF_OP_CALL */
		{ "pushtr", dt_dis_pushts },	/* DIF_OP_PUSHTR */
		{ "pushtv", dt_dis_pushts },	/* DIF_OP_PUSHTV */
		{ "popts", dt_dis_str },	/* DIF_OP_POPTS */
		{ "flushts", dt_dis_str },	/* DIF_OP_FLUSHTS */
		{ "ldgaa", dt_dis_ldv },	/* DIF_OP_LDGAA */
		{ "ldtaa", dt_dis_ldv },	/* DIF_OP_LDTAA */
		{ "stgaa", dt_dis_stv },	/* DIF_OP_STGAA */
		{ "sttaa", dt_dis_stv },	/* DIF_OP_STTAA */
		{ "ldls", dt_dis_ldv },		/* DIF_OP_LDLS */
		{ "stls", dt_dis_stv },		/* DIF_OP_STLS */
		{ "allocs", dt_dis_r1rd },	/* DIF_OP_ALLOCS */
		{ "copys", dt_dis_log },	/* DIF_OP_COPYS */
		{ "stb", dt_dis_store },	/* DIF_OP_STB */
		{ "sth", dt_dis_store },	/* DIF_OP_STH */
		{ "stw", dt_dis_store },	/* DIF_OP_STW */
		{ "stx", dt_dis_store },	/* DIF_OP_STX */
		{ "uldsb", dt_dis_load },	/* DIF_OP_ULDSB */
		{ "uldsh", dt_dis_load },	/* DIF_OP_ULDSH */
		{ "uldsw", dt_dis_load },	/* DIF_OP_ULDSW */
		{ "uldub", dt_dis_load },	/* DIF_OP_ULDUB */
		{ "ulduh", dt_dis_load },	/* DIF_OP_ULDUH */
		{ "ulduw", dt_dis_load },	/* DIF_OP_ULDUW */
		{ "uldx", dt_dis_load },	/* DIF_OP_ULDX */
		{ "rldsb", dt_dis_load },	/* DIF_OP_RLDSB */
		{ "rldsh", dt_dis_load },	/* DIF_OP_RLDSH */
		{ "rldsw", dt_dis_load },	/* DIF_OP_RLDSW */
		{ "rldub", dt_dis_load },	/* DIF_OP_RLDUB */
		{ "rlduh", dt_dis_load },	/* DIF_OP_RLDUH */
		{ "rlduw", dt_dis_load },	/* DIF_OP_RLDUW */
		{ "rldx", dt_dis_load },	/* DIF_OP_RLDX */
	};

	const struct opent *op;
	ulong_t i = 0;
	char type[64];

	(void) fprintf(fp, "DIFO 0x%p refcnt=%u returns %s\n",
	    (void *)dp, dp->dtdo_refcnt,
	    dt_dis_typestr(&dp->dtdo_rtype, type, sizeof (type)));

	(void) fprintf(fp, "%-3s %-8s    %s\n",
	    "OFF", "OPCODE", "INSTRUCTION");

	for (i = 0; i < dp->dtdo_len; i++) {
		dif_instr_t instr = dp->dtdo_buf[i];
		dif_instr_t opcode = DIF_INSTR_OP(instr);

		if (opcode >= sizeof (optab) / sizeof (optab[0]))
			opcode = 0; /* force invalid opcode message */

		op = &optab[opcode];
		(void) fprintf(fp, "%02lu: %08x    ", i, instr);
		op->op_func(dp, op->op_name, instr, fp);
		(void) fprintf(fp, "\n");
	}

	if (dp->dtdo_varlen != 0) {
		(void) fprintf(fp, "\n%-16s %-4s %-3s %-3s %-4s %s\n",
		    "NAME", "ID", "KND", "SCP", "FLAG", "TYPE");
	}

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t *v = &dp->dtdo_vartab[i];
		char kind[4], scope[4], flags[16] = { 0 };

		switch (v->dtdv_kind) {
		case DIFV_KIND_ARRAY:
			(void) strcpy(kind, "arr");
			break;
		case DIFV_KIND_SCALAR:
			(void) strcpy(kind, "scl");
			break;
		default:
			(void) snprintf(kind, sizeof (kind),
			    "%u", v->dtdv_kind);
		}

		switch (v->dtdv_scope) {
		case DIFV_SCOPE_GLOBAL:
			(void) strcpy(scope, "glb");
			break;
		case DIFV_SCOPE_THREAD:
			(void) strcpy(scope, "tls");
			break;
		case DIFV_SCOPE_LOCAL:
			(void) strcpy(scope, "loc");
			break;
		default:
			(void) snprintf(scope, sizeof (scope),
			    "%u", v->dtdv_scope);
		}

		if (v->dtdv_flags & ~(DIFV_F_REF | DIFV_F_MOD)) {
			(void) snprintf(flags, sizeof (flags), "/0x%x",
			    v->dtdv_flags & ~(DIFV_F_REF | DIFV_F_MOD));
		}

		if (v->dtdv_flags & DIFV_F_REF)
			(void) strcat(flags, "/r");
		if (v->dtdv_flags & DIFV_F_MOD)
			(void) strcat(flags, "/w");

		(void) fprintf(fp, "%-16s %-4x %-3s %-3s %-4s %s\n",
		    &dp->dtdo_strtab[v->dtdv_name],
		    v->dtdv_id, kind, scope, flags + 1,
		    dt_dis_typestr(&v->dtdv_type, type, sizeof (type)));
	}

	if (dp->dtdo_krelen != 0)
		dt_dis_rtab("KREL", dp, fp, dp->dtdo_kreltab, dp->dtdo_krelen);

	if (dp->dtdo_urelen != 0)
		dt_dis_rtab("UREL", dp, fp, dp->dtdo_ureltab, dp->dtdo_urelen);
}
