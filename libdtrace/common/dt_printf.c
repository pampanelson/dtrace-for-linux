/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)dt_printf.c	1.12	04/12/18 SMI"

#include <linux_types.h>
#include <sys/sysmacros.h>
#include <strings.h>
#include <stdlib.h>
#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include <dt_printf.h>
#include <dt_string.h>
#include <dt_impl.h>

/*ARGSUSED*/
static int
pfcheck_addr(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	return (dt_node_is_pointer(dnp) || dt_node_is_integer(dnp));
}

/*ARGSUSED*/
static int
pfcheck_str(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	ctf_file_t *ctfp;
	ctf_encoding_t e;
	ctf_arinfo_t r;
	ctf_id_t base;
	uint_t kind;

	if (dt_node_is_string(dnp))
		return (1);

	ctfp = dnp->dn_ctfp;
	base = ctf_type_resolve(ctfp, dnp->dn_type);
	kind = ctf_type_kind(ctfp, base);

	return (kind == CTF_K_ARRAY && ctf_array_info(ctfp, base, &r) == 0 &&
	    (base = ctf_type_resolve(ctfp, r.ctr_contents)) != CTF_ERR &&
	    ctf_type_encoding(ctfp, base, &e) == 0 && IS_CHAR(e));
}

/*ARGSUSED*/
static int
pfcheck_wstr(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_id_t base = ctf_type_resolve(ctfp, dnp->dn_type);
	uint_t kind = ctf_type_kind(ctfp, base);

	ctf_encoding_t e;
	ctf_arinfo_t r;

	return (kind == CTF_K_ARRAY && ctf_array_info(ctfp, base, &r) == 0 &&
	    (base = ctf_type_resolve(ctfp, r.ctr_contents)) != CTF_ERR &&
	    ctf_type_kind(ctfp, base) == CTF_K_INTEGER &&
	    ctf_type_encoding(ctfp, base, &e) == 0 && e.cte_bits == 32);
}

/*ARGSUSED*/
static int
pfcheck_csi(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	return (dt_node_is_integer(dnp) &&
	    dt_node_type_size(dnp) <= sizeof (int));
}

/*ARGSUSED*/
static int
pfcheck_fp(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	return (dt_node_is_float(dnp));
}

/*ARGSUSED*/
static int
pfcheck_xint(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	return (dt_node_is_integer(dnp));
}

static int
pfcheck_dint(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	if (dnp->dn_flags & DT_NF_SIGNED)
		pfd->pfd_flags |= DT_PFCONV_SIGNED;
	else
		pfd->pfd_fmt[strlen(pfd->pfd_fmt) - 1] = 'u';

	return (dt_node_is_integer(dnp));
}

/*ARGSUSED*/
static int
pfcheck_xshort(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_id_t type = ctf_type_resolve(ctfp, dnp->dn_type);
	char n[DT_TYPE_NAMELEN];

	return (ctf_type_name(ctfp, type, n, sizeof (n)) != NULL && (
	    strcmp(n, "short") == 0 || strcmp(n, "signed short") == 0 ||
	    strcmp(n, "unsigned short") == 0));
}

/*ARGSUSED*/
static int
pfcheck_xlong(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_id_t type = ctf_type_resolve(ctfp, dnp->dn_type);
	char n[DT_TYPE_NAMELEN];

	return (ctf_type_name(ctfp, type, n, sizeof (n)) != NULL && (
	    strcmp(n, "long") == 0 || strcmp(n, "signed long") == 0 ||
	    strcmp(n, "unsigned long") == 0));
}

/*ARGSUSED*/
static int
pfcheck_xlonglong(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_id_t type = dnp->dn_type;
	char n[DT_TYPE_NAMELEN];

	if (ctf_type_name(ctfp, ctf_type_resolve(ctfp, type), n,
	    sizeof (n)) != NULL && (strcmp(n, "long long") == 0 ||
	    strcmp(n, "signed long long") == 0 ||
	    strcmp(n, "unsigned long long") == 0))
		return (1);

	/*
	 * If the type used for %llx or %llX is not an [unsigned] long long, we
	 * also permit it to be a [u]int64_t or any typedef thereof.  We know
	 * that these typedefs are guaranteed to work with %ll[xX] in either
	 * compilation environment even though they alias to "long" in LP64.
	 */
	while (ctf_type_kind(ctfp, type) == CTF_K_TYPEDEF) {
		if (ctf_type_name(ctfp, type, n, sizeof (n)) != NULL &&
		    (strcmp(n, "int64_t") == 0 || strcmp(n, "uint64_t") == 0))
			return (1);

		type = ctf_type_reference(ctfp, type);
	}

	return (0);
}

static int
pfcheck_type(dt_pfargd_t *pfd, dt_node_t *dnp)
{
	return (ctf_type_compat(dnp->dn_ctfp, ctf_type_resolve(dnp->dn_ctfp,
	    dnp->dn_type), pfd->pfd_conv->pfc_dctfp, pfd->pfd_conv->pfc_dtype));
}

/*ARGSUSED*/
static int
pfprint_sint(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t unormal)
{
	int64_t normal = (int64_t)unormal;
	int32_t n = (int32_t)normal;

	switch (size) {
	case sizeof (int8_t):
		return (dt_printf(dtp, fp, format,
		    (int32_t)*((int8_t *)addr) / n));
	case sizeof (int16_t):
		return (dt_printf(dtp, fp, format,
		    (int32_t)*((int16_t *)addr) / n));
	case sizeof (int32_t):
		return (dt_printf(dtp, fp, format,
		    *((int32_t *)addr) / n));
	case sizeof (int64_t):
		return (dt_printf(dtp, fp, format,
		    *((int64_t *)addr) / normal));
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}
}

/*ARGSUSED*/
static int
pfprint_uint(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	uint32_t n = (uint32_t)normal;

	switch (size) {
	case sizeof (uint8_t):
		return (dt_printf(dtp, fp, format,
		    (uint32_t)*((uint8_t *)addr) / n));
	case sizeof (uint16_t):
		return (dt_printf(dtp, fp, format,
		    (uint32_t)*((uint16_t *)addr) / n));
	case sizeof (uint32_t):
		return (dt_printf(dtp, fp, format,
		    *((uint32_t *)addr) / n));
	case sizeof (uint64_t):
		return (dt_printf(dtp, fp, format,
		    *((uint64_t *)addr) / normal));
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}
}

static int
pfprint_dint(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	if (pfd->pfd_flags & DT_PFCONV_SIGNED)
		return (pfprint_sint(dtp, fp, format, pfd, addr, size, normal));
	else
		return (pfprint_uint(dtp, fp, format, pfd, addr, size, normal));
}

/*ARGSUSED*/
static int
pfprint_fp(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	double n = (double)normal;
	long double ldn = (long double)normal;

	switch (size) {
	case sizeof (float):
		return (dt_printf(dtp, fp, format,
		    (double)*((float *)addr) / n));
	case sizeof (double):
		return (dt_printf(dtp, fp, format,
		    *((double *)addr) / n));
	case sizeof (long double):
		return (dt_printf(dtp, fp, format,
		    *((long double *)addr) / ldn));
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}
}

/*ARGSUSED*/
static int
pfprint_addr(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	dtrace_syminfo_t dts;
	GElf_Sym sym;
	GElf_Addr val;

	size_t n = 20; /* for 0x%llx\0 */
	char *s;
	int err;

	switch (size) {
	case sizeof (uint32_t):
		val = *((uint32_t *)addr);
		break;
	case sizeof (uint64_t):
		val = *((uint64_t *)addr);
		break;
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}

	if ((err = dtrace_lookup_by_addr(dtp, val, &sym, &dts)) == 0)
		n += strlen(dts.dts_object) + strlen(dts.dts_name) + 2; /* +` */

	s = alloca(n);

	if (err == 0 && val != sym.st_value) {
		(void) snprintf(s, n, "%s`%s+0x%llx", dts.dts_object,
		    dts.dts_name, (u_longlong_t)val - sym.st_value);
	} else if (err == 0) {
		(void) snprintf(s, n, "%s`%s",
		    dts.dts_object, dts.dts_name);
	} else {
		/*
		 * We'll repeat the lookup, but this time we'll specify a NULL
		 * GElf_Sym -- indicating that we're only interested in the
		 * containing module.
		 */
		if (dtrace_lookup_by_addr(dtp, val, NULL, &dts) == 0) {
			(void) snprintf(s, n, "%s`0x%llx", dts.dts_object,
			    (u_longlong_t)val);
		} else {
			(void) snprintf(s, n, "0x%llx", (u_longlong_t)val);
		}
	}

	return (dt_printf(dtp, fp, format, s));
}

/*ARGSUSED*/
static int
pfprint_uaddr(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	u_longlong_t val;
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	char name[PATH_MAX], objname[PATH_MAX], c[PATH_MAX * 2];
	struct ps_prochandle *P = NULL;
	pid_t pid;
	GElf_Sym sym;
	char *obj;

	switch (size) {
	case sizeof (uint32_t):
		val = (u_longlong_t)*((uint32_t *)addr);
		break;
	case sizeof (uint64_t):
		val = (u_longlong_t)*((uint64_t *)addr);
		break;
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}

	if (dtp->dt_vector == NULL && idp != NULL && (pid = idp->di_id) != 0)
		P = dt_proc_grab(dtp, pid, PGRAB_RDONLY | PGRAB_FORCE, 0);

	if (P == NULL) {
		(void) snprintf(c, sizeof (c), "0x%llx", val);
		return (dt_printf(dtp, fp, format, c));
	}

	dt_proc_lock(dtp, P);

	if (Plookup_by_addr(P, val, name, sizeof (name), &sym) == 0) {
		(void) Pobjname(P, val, objname, sizeof (objname));

		obj = dt_basename(objname);

		if (val > sym.st_value) {
			(void) snprintf(c, sizeof (c), "%s`%s+0x%llx", obj,
			    name, (u_longlong_t)(val - sym.st_value));
		} else {
			(void) snprintf(c, sizeof (c), "%s`%s", obj, name);
		}
	} else if (Pobjname(P, val, objname, sizeof (objname)) != NULL) {
		(void) snprintf(c, sizeof (c), "%s`0x%llx",
		    dt_basename(objname), val);
	} else {
		(void) snprintf(c, sizeof (c), "0x%llx", val);
	}

	dt_proc_unlock(dtp, P);
	dt_proc_release(dtp, P);

	return (dt_printf(dtp, fp, format, c));
}

/*ARGSUSED*/
static int
pfprint_stack(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *vaddr, size_t size, uint64_t normal)
{
	int depth = size / sizeof (pc_t), width;
	dtrace_optval_t saved = dtp->dt_options[DTRACEOPT_STACKINDENT];
	const dtrace_recdesc_t *rec = pfd->pfd_rec;
	caddr_t addr = (caddr_t)vaddr;
	int err = 0;

	/*
	 * We have stashed the value of the STACKINDENT option, and we will
	 * now override it for the purposes of formatting the stack.  If the
	 * field has been specified as left-aligned (i.e. (%-#), we set the
	 * indentation to be the width.  This is a slightly odd semantic, but
	 * it's useful functionality -- and it's slightly odd to begin with to
	 * be using a single format specifier to be formatting multiple lines
	 * of text...
	 */
	if (pfd->pfd_dynwidth < 0) {
		assert(pfd->pfd_flags & DT_PFCONV_DYNWIDTH);
		width = -pfd->pfd_dynwidth;
	} else if (pfd->pfd_flags & DT_PFCONV_LEFT) {
		width = pfd->pfd_dynwidth ? pfd->pfd_dynwidth : pfd->pfd_width;
	} else {
		width = 0;
	}

	dtp->dt_options[DTRACEOPT_STACKINDENT] = width;

	switch (rec->dtrd_action) {
	case DTRACEACT_USTACK:
	case DTRACEACT_JSTACK:
		err = dt_print_ustack(dtp, fp, format, addr, rec->dtrd_arg);
		break;

	case DTRACEACT_STACK:
		err = dt_print_stack(dtp, fp, format, addr, depth);
		break;

	default:
		assert(0);
	}

	dtp->dt_options[DTRACEOPT_STACKINDENT] = saved;

	return (err);
}

/*ARGSUSED*/
static int
pfprint_time(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	char src[32], buf[32], *dst = buf;
	hrtime_t time = *((uint64_t *)addr);
	time_t sec = (time_t)(time / NANOSEC);
	int i;

	/*
	 * ctime(3C) returns a string of the form "Dec  3 17:20:00 1973\n\0".
	 * Below, we turn this into the canonical adb/mdb /[yY] format,
	 * "1973 Dec  3 17:20:00".
	 */
# if linux
	(void) ctime_r(&sec, src);
# else
	(void) ctime_r(&sec, src, sizeof (src));
# endif

	/*
	 * Place the 4-digit year at the head of the string...
	 */
	for (i = 20; i < 24; i++)
		*dst++ = src[i];

	/*
	 * ...and follow it with the remainder (month, day, hh:mm:ss).
	 */
	for (i = 3; i < 19; i++)
		*dst++ = src[i];

	*dst = '\0';
	return (dt_printf(dtp, fp, format, buf));
}

/*
 * This prints the time in RFC 822 standard form.  This is useful for emitting
 * notions of time that are consumed by standard tools (e.g., as part of an
 * RSS feed).
 */
/*ARGSUSED*/
static int
pfprint_time822(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	hrtime_t time = *((uint64_t *)addr);
	time_t sec = (time_t)(time / NANOSEC);
	struct tm tm;
	char buf[64];

	(void) localtime_r(&sec, &tm);
	(void) strftime(buf, sizeof (buf), "%a, %d %b %G %T %Z", &tm);
	return (dt_printf(dtp, fp, format, buf));
}

/*ARGSUSED*/
static int
pfprint_cstr(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	char *s = alloca(size + 1);

	bcopy(addr, s, size);
	s[size] = '\0';
	return (dt_printf(dtp, fp, format, s));
}

/*ARGSUSED*/
static int
pfprint_wstr(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	wchar_t *ws = alloca(size + sizeof (wchar_t));

	bcopy(addr, ws, size);
	ws[size / sizeof (wchar_t)] = L'\0';
	return (dt_printf(dtp, fp, format, ws));
}

/*ARGSUSED*/
static int
pfprint_estr(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	char *s;
	int n;

	if ((s = strchr2esc(addr, size)) == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	n = dt_printf(dtp, fp, format, s);
	free(s);
	return (n);
}

static int
pfprint_echr(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	char c;

	switch (size) {
	case sizeof (int8_t):
		c = *(int8_t *)addr;
		break;
	case sizeof (int16_t):
		c = *(int16_t *)addr;
		break;
	case sizeof (int32_t):
		c = *(int32_t *)addr;
		break;
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}

	return (pfprint_estr(dtp, fp, format, pfd, &c, 1, normal));
}

/*ARGSUSED*/
static int
pfprint_pct(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	return (dt_printf(dtp, fp, "%%"));
}

static const char pfproto_xint[] = "char, short, int, long, or long long";
static const char pfproto_csi[] = "char, short, or int";
static const char pfproto_fp[] = "float, double, or long double";
static const char pfproto_addr[] = "pointer or integer";
static const char pfproto_cstr[] = "char [] or string (or use stringof)";
static const char pfproto_wstr[] = "wchar_t []";

/*
 * Printf format conversion dictionary.  This table should match the set of
 * conversions offered by printf(3C), as well as some additional extensions.
 * The second parameter is an ASCII string which is either an actual type
 * name we should look up (if pfcheck_type is specified), or just a descriptive
 * string of the types expected for use in error messages.
 */
static const dt_pfconv_t _dtrace_conversions[] = {
{ "a", "s", pfproto_addr, pfcheck_addr, pfprint_addr },
{ "A", "s", pfproto_addr, pfcheck_addr, pfprint_uaddr },
{ "c", "c", pfproto_csi, pfcheck_csi, pfprint_sint },
{ "C", "s", pfproto_csi, pfcheck_csi, pfprint_echr },
{ "d", "d", pfproto_xint, pfcheck_dint, pfprint_dint },
{ "e", "e", pfproto_fp, pfcheck_fp, pfprint_fp },
{ "E", "E", pfproto_fp, pfcheck_fp, pfprint_fp },
{ "f", "f", pfproto_fp, pfcheck_fp, pfprint_fp },
{ "g", "g", pfproto_fp, pfcheck_fp, pfprint_fp },
{ "G", "G", pfproto_fp, pfcheck_fp, pfprint_fp },
{ "hd", "d", "short", pfcheck_type, pfprint_sint },
{ "hi", "i", "short", pfcheck_type, pfprint_sint },
{ "ho", "o", "unsigned short", pfcheck_type, pfprint_uint },
{ "hu", "u", "unsigned short", pfcheck_type, pfprint_uint },
{ "hx", "x", "short", pfcheck_xshort, pfprint_uint },
{ "hX", "X", "short", pfcheck_xshort, pfprint_uint },
{ "i", "i", pfproto_xint, pfcheck_dint, pfprint_dint },
{ "k", "s", "stack", pfcheck_type, pfprint_stack },
{ "lc", "lc", "int", pfcheck_type, pfprint_sint }, /* a.k.a. wint_t */
{ "ld",	"d", "long", pfcheck_type, pfprint_sint },
{ "li",	"i", "long", pfcheck_type, pfprint_sint },
{ "lo",	"o", "unsigned long", pfcheck_type, pfprint_uint },
{ "lu", "u", "unsigned long", pfcheck_type, pfprint_uint },
{ "ls",	"ls", pfproto_wstr, pfcheck_wstr, pfprint_wstr },
{ "lx",	"x", "long", pfcheck_xlong, pfprint_uint },
{ "lX",	"X", "long", pfcheck_xlong, pfprint_uint },
{ "lld", "d", "long long", pfcheck_type, pfprint_sint },
{ "lli", "i", "long long", pfcheck_type, pfprint_sint },
{ "llo", "o", "unsigned long long", pfcheck_type, pfprint_uint },
{ "llu", "u", "unsigned long long", pfcheck_type, pfprint_uint },
{ "llx", "x", "long long", pfcheck_xlonglong, pfprint_uint },
{ "llX", "X", "long long", pfcheck_xlonglong, pfprint_uint },
{ "Le",	"e", "long double", pfcheck_type, pfprint_fp },
{ "LE",	"E", "long double", pfcheck_type, pfprint_fp },
{ "Lf",	"f", "long double", pfcheck_type, pfprint_fp },
{ "Lg",	"g", "long double", pfcheck_type, pfprint_fp },
{ "LG",	"G", "long double", pfcheck_type, pfprint_fp },
{ "o", "o", pfproto_xint, pfcheck_xint, pfprint_uint },
{ "p", "x", pfproto_addr, pfcheck_addr, pfprint_uint },
{ "s", "s", "char [] or string (or use stringof)", pfcheck_str, pfprint_cstr },
{ "S", "s", pfproto_cstr, pfcheck_str, pfprint_estr },
{ "T", "s", "uint64_t", pfcheck_type, pfprint_time822 },
{ "u", "u", pfproto_xint, pfcheck_xint, pfprint_uint },
{ "wc",	"wc", "int", pfcheck_type, pfprint_sint }, /* a.k.a. wchar_t */
{ "ws", "ws", pfproto_wstr, pfcheck_wstr, pfprint_wstr },
{ "x", "x", pfproto_xint, pfcheck_xint, pfprint_uint },
{ "X", "X", pfproto_xint, pfcheck_xint, pfprint_uint },
{ "Y", "s", "uint64_t", pfcheck_type, pfprint_time },
{ "%", "%", "void", pfcheck_type, pfprint_pct },
{ NULL, NULL, NULL, NULL, NULL }
};

int
dt_pfdict_create(dtrace_hdl_t *dtp)
{
	uint_t n = _dtrace_strbuckets;
	const dt_pfconv_t *pfd;
	dt_pfdict_t *pdi;

	if ((pdi = malloc(sizeof (dt_pfdict_t))) == NULL ||
	    (pdi->pdi_buckets = malloc(sizeof (dt_pfconv_t *) * n)) == NULL) {
		free(pdi);
		return (dt_set_errno(dtp, EDT_NOMEM));
	}

	dtp->dt_pfdict = pdi;
	bzero(pdi->pdi_buckets, sizeof (dt_pfconv_t *) * n);
	pdi->pdi_nbuckets = n;

	for (pfd = _dtrace_conversions; pfd->pfc_name != NULL; pfd++) {
		dtrace_typeinfo_t dtt;
		dt_pfconv_t *pfc;
		uint_t h;

		if ((pfc = malloc(sizeof (dt_pfconv_t))) == NULL) {
			dt_pfdict_destroy(dtp);
			return (dt_set_errno(dtp, EDT_NOMEM));
		}

		bcopy(pfd, pfc, sizeof (dt_pfconv_t));
		h = dt_strtab_hash(pfc->pfc_name, NULL) % n;
		pfc->pfc_next = pdi->pdi_buckets[h];
		pdi->pdi_buckets[h] = pfc;

		dtt.dtt_ctfp = NULL;
		dtt.dtt_type = CTF_ERR;

		/*
		 * The "D" container or its parent must contain a definition of
		 * any type referenced by a printf conversion.  If none can be
		 * found, we fail to initialize the printf dictionary.
		 */
		if (pfc->pfc_check == &pfcheck_type && dtrace_lookup_by_type(
		    dtp, DTRACE_OBJ_DDEFS, pfc->pfc_tstr, &dtt) != 0) {
			dt_pfdict_destroy(dtp);
			return (dt_set_errno(dtp, EDT_NOCONV));
		}

		pfc->pfc_dctfp = dtt.dtt_ctfp;
		pfc->pfc_dtype = dtt.dtt_type;

		/*
		 * The "C" container may contain an alternate definition of an
		 * explicit conversion type.  If it does, use it; otherwise
		 * just set pfc_ctype to pfc_dtype so it is always valid.
		 */
		if (pfc->pfc_check == &pfcheck_type && dtrace_lookup_by_type(
		    dtp, DTRACE_OBJ_CDEFS, pfc->pfc_tstr, &dtt) == 0) {
			pfc->pfc_cctfp = dtt.dtt_ctfp;
			pfc->pfc_ctype = dtt.dtt_type;
		} else {
			pfc->pfc_cctfp = pfc->pfc_dctfp;
			pfc->pfc_ctype = pfc->pfc_dtype;
		}

		if (pfc->pfc_check == NULL || pfc->pfc_print == NULL ||
		    pfc->pfc_ofmt == NULL || pfc->pfc_tstr == NULL) {
			dt_pfdict_destroy(dtp);
			return (dt_set_errno(dtp, EDT_BADCONV));
		}

		dt_dprintf("loaded printf conversion %%%s\n", pfc->pfc_name);
	}

	return (0);
}

void
dt_pfdict_destroy(dtrace_hdl_t *dtp)
{
	dt_pfdict_t *pdi = dtp->dt_pfdict;
	dt_pfconv_t *pfc, *nfc;
	uint_t i;

	if (pdi == NULL)
		return;

	for (i = 0; i < pdi->pdi_nbuckets; i++) {
		for (pfc = pdi->pdi_buckets[i]; pfc != NULL; pfc = nfc) {
			nfc = pfc->pfc_next;
			free(pfc);
		}
	}

	free(pdi->pdi_buckets);
	free(pdi);
	dtp->dt_pfdict = NULL;
}

static const dt_pfconv_t *
dt_pfdict_lookup(dtrace_hdl_t *dtp, const char *name)
{
	dt_pfdict_t *pdi = dtp->dt_pfdict;
	uint_t h = dt_strtab_hash(name, NULL) % pdi->pdi_nbuckets;
	const dt_pfconv_t *pfc;

	for (pfc = pdi->pdi_buckets[h]; pfc != NULL; pfc = pfc->pfc_next) {
		if (strcmp(pfc->pfc_name, name) == 0)
			break;
	}

	return (pfc);
}

static dt_pfargv_t *
dt_printf_error(dtrace_hdl_t *dtp, int err)
{
	if (yypcb != NULL)
		longjmp(yypcb->pcb_jmpbuf, err);

	(void) dt_set_errno(dtp, err);
	return (NULL);
}

dt_pfargv_t *
dt_printf_create(dtrace_hdl_t *dtp, const char *s)
{
	dt_pfargd_t *pfd, *nfd = NULL;
	dt_pfargv_t *pfv;
	const char *p, *q;
	char *format;

	if ((pfv = malloc(sizeof (dt_pfargv_t))) == NULL ||
	    (format = strdup(s)) == NULL) {
		free(pfv);
		return (dt_printf_error(dtp, EDT_NOMEM));
	}

	pfv->pfv_format = format;
	pfv->pfv_argv = NULL;
	pfv->pfv_argc = 0;
	pfv->pfv_flags = 0;

	for (q = format; (p = strchr(q, '%')) != NULL; q = *p ? p + 1 : p) {
		uint_t namelen = 0;
		int digits = 0;
		int dot = 0;

		char name[8];
		char c;
		int n;

		if ((pfd = malloc(sizeof (dt_pfargd_t))) == NULL) {
			dt_printf_destroy(pfv);
			return (dt_printf_error(dtp, EDT_NOMEM));
		}

		if (pfv->pfv_argv != NULL)
			nfd->pfd_next = pfd;
		else
			pfv->pfv_argv = pfd;

		bzero(pfd, sizeof (dt_pfargd_t));
		pfv->pfv_argc++;
		nfd = pfd;

		if (p > q) {
			pfd->pfd_preflen = (size_t)(p - q);
			pfd->pfd_prefix = q;
		}

		fmt_switch:
		switch (c = *++p) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (dot == 0 && digits == 0 && c == '0') {
				pfd->pfd_flags |= DT_PFCONV_ZPAD;
				pfd->pfd_flags &= ~DT_PFCONV_LEFT;
				goto fmt_switch;
			}

			for (n = 0; isdigit(c); c = *++p)
				n = n * 10 + c - '0';

			if (dot)
				pfd->pfd_prec = n;
			else
				pfd->pfd_width = n;

			p--;
			digits++;
			goto fmt_switch;

		case '#':
			pfd->pfd_flags |= DT_PFCONV_ALT;
			goto fmt_switch;

		case '*':
			n = dot ? DT_PFCONV_DYNPREC : DT_PFCONV_DYNWIDTH;

			if (pfd->pfd_flags & n) {
				yywarn("format conversion #%u has more than "
				    "one '*' specified for the output %s\n",
				    pfv->pfv_argc, n ? "precision" : "width");

				dt_printf_destroy(pfv);
				return (dt_printf_error(dtp, EDT_COMPILER));
			}

			pfd->pfd_flags |= n;
			goto fmt_switch;

		case '+':
			pfd->pfd_flags |= DT_PFCONV_SPOS;
			goto fmt_switch;

		case '-':
			pfd->pfd_flags |= DT_PFCONV_LEFT;
			pfd->pfd_flags &= ~DT_PFCONV_ZPAD;
			goto fmt_switch;

		case '.':
			if (dot++ != 0) {
				yywarn("format conversion #%u has more than "
				    "one '.' specified\n", pfv->pfv_argc);

				dt_printf_destroy(pfv);
				return (dt_printf_error(dtp, EDT_COMPILER));
			}
			digits = 0;
			goto fmt_switch;

		case '?':
			if (dtp->dt_conf.dtc_ctfmodel == CTF_MODEL_LP64)
				pfd->pfd_width = 16;
			else
				pfd->pfd_width = 8;
			goto fmt_switch;

		case '@':
			pfd->pfd_flags |= DT_PFCONV_AGG;
			goto fmt_switch;

		case '\'':
			pfd->pfd_flags |= DT_PFCONV_GROUP;
			goto fmt_switch;

		case ' ':
			pfd->pfd_flags |= DT_PFCONV_SPACE;
			goto fmt_switch;

		case '$':
			yywarn("format conversion #%u uses unsupported "
			    "positional format (%%n$)\n", pfv->pfv_argc);

			dt_printf_destroy(pfv);
			return (dt_printf_error(dtp, EDT_COMPILER));

		case '%':
			if (p[-1] == '%')
				goto default_lbl; /* if %% then use "%" conv */

			yywarn("format conversion #%u cannot be combined "
			    "with other format flags: %%%%\n", pfv->pfv_argc);

			dt_printf_destroy(pfv);
			return (dt_printf_error(dtp, EDT_COMPILER));

		case '\0':
			yywarn("format conversion #%u name expected before "
			    "end of format string\n", pfv->pfv_argc);

			dt_printf_destroy(pfv);
			return (dt_printf_error(dtp, EDT_COMPILER));

		case 'h':
		case 'l':
		case 'L':
		case 'w':
			if (namelen < sizeof (name) - 2)
				name[namelen++] = c;
			goto fmt_switch;

		default_lbl:
		default:
			name[namelen++] = c;
			name[namelen] = '\0';
		}

		if (strcmp(name, "A") == 0) {
			dt_ident_t *idp;

			idp = dt_idhash_lookup(dtp->dt_macros, "target");

			if (idp == NULL || idp->di_id == 0) {
				yywarn("format conversion #%u only "
				    "valid when target process is specified\n",
				    pfv->pfv_argc);

				dt_printf_destroy(pfv);
				return (dt_printf_error(dtp, EDT_COMPILER));
			}
		}

		pfd->pfd_conv = dt_pfdict_lookup(dtp, name);

		if (pfd->pfd_conv == NULL) {
			yywarn("format conversion #%u is undefined: %%%s\n",
			    pfv->pfv_argc, name);
			dt_printf_destroy(pfv);
			return (dt_printf_error(dtp, EDT_COMPILER));
		}
	}

	if (*q != '\0' || *format == '\0') {
		if ((pfd = malloc(sizeof (dt_pfargd_t))) == NULL) {
			dt_printf_destroy(pfv);
			return (dt_printf_error(dtp, EDT_NOMEM));
		}

		if (pfv->pfv_argv != NULL)
			nfd->pfd_next = pfd;
		else
			pfv->pfv_argv = pfd;

		bzero(pfd, sizeof (dt_pfargd_t));
		pfv->pfv_argc++;

		pfd->pfd_prefix = q;
		pfd->pfd_preflen = strlen(q);
	}

	return (pfv);
}

void
dt_printf_destroy(dt_pfargv_t *pfv)
{
	dt_pfargd_t *pfd, *nfd;

	for (pfd = pfv->pfv_argv; pfd != NULL; pfd = nfd) {
		nfd = pfd->pfd_next;
		free(pfd);
	}

	free(pfv->pfv_format);
	free(pfv);
}

void
dt_printf_validate(dt_pfargv_t *pfv, uint_t flags,
    dt_ident_t *idp, int foff, dtrace_actkind_t kind, dt_node_t *dnp)
{
	dt_pfargd_t *pfd = pfv->pfv_argv;
	const char *func = idp->di_name;

	char n[DT_TYPE_NAMELEN];
	dtrace_typeinfo_t dtt;
	const char *aggtype;
	dt_node_t aggnode;
	int i, j;

	if (pfv->pfv_format[0] == '\0') {
		xyerror(D_PRINTF_FMT_EMPTY,
		    "%s( ) format string is empty\n", func);
	}

	/*
	 * We fake up a parse node representing the type that can be used with
	 * an aggregation result conversion.  For now we hardcode the signed
	 * aggregations; this will be fixed later when sign issues are fixed.
	 */
	if (kind == DTRACEAGG_QUANTIZE || kind == DTRACEAGG_LQUANTIZE)
		aggtype = "int64_t";
	else
		aggtype = "uint64_t";

	if (dt_type_lookup(aggtype, &dtt) != 0)
		xyerror(D_TYPE_ERR, "failed to lookup agg type %s\n", aggtype);

	bzero(&aggnode, sizeof (aggnode));
	dt_node_type_assign(&aggnode, dtt.dtt_ctfp, dtt.dtt_type);

	for (i = 0, j = 0; i < pfv->pfv_argc; i++, pfd = pfd->pfd_next) {
		const dt_pfconv_t *pfc = pfd->pfd_conv;
		const char *dyns[2];
		int dync = 0;

		char vname[64];
		dt_node_t *vnp;

		if (pfc == NULL)
			continue; /* no checking if argd is just a prefix */

		if (pfc->pfc_print == &pfprint_pct) {
			(void) strcat(pfd->pfd_fmt, pfc->pfc_ofmt);
			continue;
		}

		if (pfd->pfd_flags & DT_PFCONV_DYNPREC)
			dyns[dync++] = ".*";
		if (pfd->pfd_flags & DT_PFCONV_DYNWIDTH)
			dyns[dync++] = "*";

		for (; dync != 0; dync--) {
			if (dnp == NULL) {
				xyerror(D_PRINTF_DYN_PROTO,
				    "%s( ) prototype mismatch: conversion "
				    "#%d (%%%s) is missing a corresponding "
				    "\"%s\" argument\n", func, i + 1,
				    pfc->pfc_name, dyns[dync - 1]);
			}

			if (dt_node_is_integer(dnp) == 0) {
				xyerror(D_PRINTF_DYN_TYPE,
				    "%s( ) argument #%d is incompatible "
				    "with conversion #%d prototype:\n"
				    "\tconversion: %% %s %s\n"
				    "\t prototype: int\n\t  argument: %s\n",
				    func, j + foff + 1, i + 1,
				    dyns[dync - 1], pfc->pfc_name,
				    dt_node_type_name(dnp, n, sizeof (n)));
			}

			dnp = dnp->dn_list;
			j++;
		}

		/*
		 * If this conversion is consuming the aggregation data, set
		 * the value node pointer (vnp) to a fake node based on the
		 * aggregating function result type.  Otherwise assign vnp to
		 * the next parse node in the argument list, if there is one.
		 */
		if (pfd->pfd_flags & DT_PFCONV_AGG) {
			if (!(flags & DT_PRINTF_AGGREGATION)) {
				xyerror(D_PRINTF_AGG_CONV,
				    "%%@ conversion requires an aggregation"
				    " and is not for use with %s( )\n", func);
			}
			(void) strlcpy(vname, "aggregating action",
			    sizeof (vname));
			vnp = &aggnode;
		} else if (dnp == NULL) {
			xyerror(D_PRINTF_ARG_PROTO,
			    "%s( ) prototype mismatch: conversion #%d (%%"
			    "%s) is missing a corresponding value argument\n",
			    func, i + 1, pfc->pfc_name);
		} else {
			(void) snprintf(vname, sizeof (vname),
			    "argument #%d", j + foff + 1);
			vnp = dnp;
			dnp = dnp->dn_list;
			j++;
		}

		/*
		 * Fill in the proposed final format string by prepending any
		 * size-related prefixes to the pfconv's format string.  The
		 * pfc_check() function below may optionally modify the format
		 * as part of validating the type of the input argument.
		 */
		if (pfc->pfc_print == &pfprint_sint ||
		    pfc->pfc_print == &pfprint_uint ||
		    pfc->pfc_print == &pfprint_dint) {
			if (dt_node_type_size(vnp) == sizeof (uint64_t))
				(void) strcpy(pfd->pfd_fmt, "ll");
		} else if (pfc->pfc_print == &pfprint_fp) {
			if (dt_node_type_size(vnp) == sizeof (long double))
				(void) strcpy(pfd->pfd_fmt, "L");
		}

		(void) strcat(pfd->pfd_fmt, pfc->pfc_ofmt);

		/*
		 * Validate the format conversion against the value node type.
		 * If the conversion is good, create the descriptor format
		 * string by concatenating together any required printf(3C)
		 * size prefixes with the conversion's native format string.
		 */
		if (pfc->pfc_check(pfd, vnp) == 0) {
			xyerror(D_PRINTF_ARG_TYPE,
			    "%s( ) %s is incompatible with "
			    "conversion #%d prototype:\n\tconversion: %%%s\n"
			    "\t prototype: %s\n\t  argument: %s\n", func,
			    vname, i + 1, pfc->pfc_name, pfc->pfc_tstr,
			    dt_node_type_name(vnp, n, sizeof (n)));
		}
	}

	if ((flags & DT_PRINTF_EXACTLEN) && dnp != NULL) {
		xyerror(D_PRINTF_ARG_EXTRA,
		    "%s( ) prototype mismatch: only %d arguments "
		    "required by this format string\n", func, j);
	}

	pfv->pfv_flags = flags;
}

static int
dt_printf_getint(dtrace_hdl_t *dtp, const dtrace_recdesc_t *recp,
    uint_t nrecs, const void *buf, size_t len, int *ip)
{
	uintptr_t addr;

	if (nrecs == 0)
		return (dt_set_errno(dtp, EDT_DMISMATCH));

	addr = (uintptr_t)buf + recp->dtrd_offset;

	if (addr + sizeof (int) > (uintptr_t)buf + len)
		return (dt_set_errno(dtp, EDT_DOFFSET));

	if (addr & (recp->dtrd_alignment - 1))
		return (dt_set_errno(dtp, EDT_DALIGN));

	switch (recp->dtrd_size) {
	case sizeof (int8_t):
		*ip = (int)*((int8_t *)addr);
		break;
	case sizeof (int16_t):
		*ip = (int)*((int16_t *)addr);
		break;
	case sizeof (int32_t):
		*ip = (int)*((int32_t *)addr);
		break;
	case sizeof (int64_t):
		*ip = (int)*((int64_t *)addr);
		break;
	default:
		return (dt_set_errno(dtp, EDT_DMISMATCH));
	}

	return (0);
}

/*ARGSUSED*/
static int
pfprint_average(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	const uint64_t *data = addr;

	if (size != sizeof (uint64_t) * 2)
		return (dt_set_errno(dtp, EDT_DMISMATCH));

	return (dt_printf(dtp, fp, format,
	    data[0] ? data[1] / normal / data[0] : 0));
}

/*ARGSUSED*/
static int
pfprint_quantize(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	return (dt_print_quantize(dtp, fp, addr, size, normal));
}

/*ARGSUSED*/
static int
pfprint_lquantize(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    const dt_pfargd_t *pfd, const void *addr, size_t size, uint64_t normal)
{
	return (dt_print_lquantize(dtp, fp, addr, size, normal));
}

static int
dt_printf_format(dtrace_hdl_t *dtp, FILE *fp, const dt_pfargv_t *pfv,
    const dtrace_recdesc_t *recs, uint_t nrecs, const void *buf,
    size_t len, uint64_t normal)
{
	dt_pfargd_t *pfd = pfv->pfv_argv;
	const dtrace_recdesc_t *recp = recs;
	const dtrace_recdesc_t *aggr = NULL;
	uchar_t *lim = (uchar_t *)buf + len;
	char format[64] = "%";
	int i;

	/*
	 * If we are formatting an aggregation, set 'aggr' to the final record
	 * description (the aggregation result) so we can use this record with
	 * any conversion where DT_PFCONV_AGG is set.  We then decrement nrecs
	 * to prevent this record from being used with any other conversion.
	 */
	if (pfv->pfv_flags & DT_PRINTF_AGGREGATION) {
		if (nrecs == 0)
			return (dt_set_errno(dtp, EDT_DMISMATCH));
		aggr = recp + nrecs - 1;
		nrecs--;
	}

	for (i = 0; i < pfv->pfv_argc; i++, pfd = pfd->pfd_next) {
		const dt_pfconv_t *pfc = pfd->pfd_conv;
		int width = pfd->pfd_width;
		int prec = pfd->pfd_prec;
		int rval;

		char *f = format + 1; /* skip initial '%' */
		const dtrace_recdesc_t *rec;
		dt_pfprint_f *func;
		uchar_t *addr;
		size_t size;

		if (pfd->pfd_preflen != 0) {
			char *tmp = alloca(pfd->pfd_preflen + 1);

			bcopy(pfd->pfd_prefix, tmp, pfd->pfd_preflen);
			tmp[pfd->pfd_preflen] = '\0';

			if ((rval = dt_printf(dtp, fp, tmp)) < 0)
				return (rval);
		}

		if (pfc == NULL) {
			if (pfv->pfv_argc == 1)
				return (nrecs != 0);
			continue;
		}

		/*
		 * If the conversion is %%, just invoke the print callback
		 * with no data record and continue; it consumes no record.
		 */
		if (pfc->pfc_print == &pfprint_pct) {
			if (pfc->pfc_print(dtp, fp, NULL, pfd, NULL, 0, 1) >= 0)
				continue;
			return (-1); /* errno is set for us */
		}

		if (pfd->pfd_flags & DT_PFCONV_DYNWIDTH) {
			if (dt_printf_getint(dtp, recp++, nrecs--, buf,
			    len, &width) == -1)
				return (-1); /* errno is set for us */
			pfd->pfd_dynwidth = width;
		} else {
			pfd->pfd_dynwidth = 0;
		}

		if ((pfd->pfd_flags & DT_PFCONV_DYNPREC) && dt_printf_getint(
		    dtp, recp++, nrecs--, buf, len, &prec) == -1)
			return (-1); /* errno is set for us */

		if (pfd->pfd_flags & DT_PFCONV_AGG) {
			if (aggr == NULL)
				return (dt_set_errno(dtp, EDT_DMISMATCH));
			rec = aggr;
		} else {
			if (nrecs == 0)
				return (dt_set_errno(dtp, EDT_DMISMATCH));
			rec = recp++;
			nrecs--;
		}

		addr = (uchar_t *)buf + rec->dtrd_offset;
		size = rec->dtrd_size;

		if (addr + size > lim) {
			dt_dprintf("bad size: addr=%p size=0x%x lim=%p\n",
			    (void *)addr, rec->dtrd_size, (void *)lim);
			return (dt_set_errno(dtp, EDT_DOFFSET));
		}

		if (rec->dtrd_alignment != 0 &&
		    ((uintptr_t)addr & (rec->dtrd_alignment - 1)) != 0) {
			dt_dprintf("bad align: addr=%p size=0x%x align=0x%x\n",
			    (void *)addr, rec->dtrd_size, rec->dtrd_alignment);
			return (dt_set_errno(dtp, EDT_DALIGN));
		}

		switch (rec->dtrd_action) {
		case DTRACEAGG_AVG:
			func = pfprint_average;
			break;
		case DTRACEAGG_QUANTIZE:
			func = pfprint_quantize;
			break;
		case DTRACEAGG_LQUANTIZE:
			func = pfprint_lquantize;
			break;
		default:
			func = pfc->pfc_print;
			break;
		}

		if (pfd->pfd_flags & DT_PFCONV_ALT)
			*f++ = '#';
		if (pfd->pfd_flags & DT_PFCONV_ZPAD)
			*f++ = '0';
		if (pfd->pfd_flags & DT_PFCONV_LEFT)
			*f++ = '-';
		if (pfd->pfd_flags & DT_PFCONV_SPOS)
			*f++ = '+';
		if (pfd->pfd_flags & DT_PFCONV_GROUP)
			*f++ = '\'';
		if (pfd->pfd_flags & DT_PFCONV_SPACE)
			*f++ = ' ';

		/*
		 * If we're printing a stack and DT_PFCONV_LEFT is set, we
		 * don't add the width to the format string.  See the block
		 * comment in pfprint_stack() for a description of the
		 * behavior in this case.
		 */
		if (func == pfprint_stack && (pfd->pfd_flags & DT_PFCONV_LEFT))
			width = 0;

		if (width != 0)
			f += snprintf(f, sizeof (format), "%d", width);

		if (prec != 0)
			f += snprintf(f, sizeof (format), ".%d", prec);

		(void) strcpy(f, pfd->pfd_fmt);
		pfd->pfd_rec = rec;

		if (func(dtp, fp, format, pfd, addr, size,
		    rec == aggr ? normal : 1) < 0)
			return (-1); /* errno is set for us */
	}

	return ((int)(recp - recs));
}

int
dtrace_sprintf(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_recdesc_t *recp, uint_t nrecs, const void *buf, size_t len)
{
	dtrace_optval_t size;
	int rval;

	rval = dtrace_getopt(dtp, "strsize", &size);
	assert(rval == 0);
	assert(dtp->dt_sprintf_buflen == 0);

	if (dtp->dt_sprintf_buf != NULL)
		free(dtp->dt_sprintf_buf);

	if ((dtp->dt_sprintf_buf = malloc(size)) == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	bzero(dtp->dt_sprintf_buf, size);
	dtp->dt_sprintf_buflen = size;
	rval = dt_printf_format(dtp, fp, fmtdata, recp, nrecs, buf, len, 1);
	dtp->dt_sprintf_buflen = 0;

	if (rval == -1)
		free(dtp->dt_sprintf_buf);

	return (rval);
}

int
dtrace_system(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_recdesc_t *recp, uint_t nrecs, const void *buf, size_t len)
{
	int rval = dtrace_sprintf(dtp, fp, fmtdata, recp, nrecs, buf, len);

	if (rval == -1)
		return (rval);

	/*
	 * Before we execute the specified command, flush fp to assure that
	 * any prior dt_printf()'s appear before the output of the command
	 * not after it.
	 */
	(void) fflush(fp);

	if (system(dtp->dt_sprintf_buf) == -1)
		return (dt_set_errno(dtp, errno));

	return (rval);
}

int
dtrace_fprintf(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_recdesc_t *recp, uint_t nrecs, const void *buf, size_t len)
{
	return (dt_printf_format(dtp, fp, fmtdata, recp, nrecs, buf, len, 1));
}

void *
dtrace_printf_create(dtrace_hdl_t *dtp, const char *s)
{
	dt_pfargv_t *pfv = dt_printf_create(dtp, s);
	dt_pfargd_t *pfd;
	int i;

	if (pfv == NULL)
		return (NULL);		/* errno has been set for us */

	pfd = pfv->pfv_argv;

	for (i = 0; i < pfv->pfv_argc; i++, pfd = pfd->pfd_next) {
		const dt_pfconv_t *pfc = pfd->pfd_conv;

		if (pfc == NULL)
			continue;

		/*
		 * If the output format is not %s then we assume that we have
		 * been given a correctly-sized format string, so we copy the
		 * true format name including the size modifier.  If the output
		 * format is %s, then either the input format is %s as well or
		 * it is one of our custom formats (e.g. pfprint_addr), so we
		 * must set pfd_fmt to be the output format conversion "s".
		 */
		if (strcmp(pfc->pfc_ofmt, "s") != 0)
			(void) strcat(pfd->pfd_fmt, pfc->pfc_name);
		else
			(void) strcat(pfd->pfd_fmt, pfc->pfc_ofmt);
	}

	return (pfv);
}

void *
dtrace_printa_create(dtrace_hdl_t *dtp, const char *s)
{
	dt_pfargv_t *pfv = dtrace_printf_create(dtp, s);

	if (pfv == NULL)
		return (NULL);		/* errno has been set for us */

	pfv->pfv_flags |= DT_PRINTF_AGGREGATION;

	return (pfv);
}

/*ARGSUSED*/
size_t
dtrace_printf_format(dtrace_hdl_t *dtp, void *fmtdata, char *s, size_t len)
{
	dt_pfargv_t *pfv = fmtdata;
	dt_pfargd_t *pfd = pfv->pfv_argv;

	/*
	 * An upper bound on the string length is the length of the original
	 * format string, plus three times the number of conversions (each
	 * conversion could add up an additional "ll" and/or pfd_width digit
	 * in the case of converting %? to %16) plus one for a terminating \0.
	 */
	size_t formatlen = strlen(pfv->pfv_format) + 3 * pfv->pfv_argc + 1;
	char *format = alloca(formatlen);
	char *f = format;
	int i, j;

	for (i = 0; i < pfv->pfv_argc; i++, pfd = pfd->pfd_next) {
		const dt_pfconv_t *pfc = pfd->pfd_conv;
		const char *str;
		int width = pfd->pfd_width;
		int prec = pfd->pfd_prec;

		if (pfd->pfd_preflen != 0) {
			for (j = 0; j < pfd->pfd_preflen; j++)
				*f++ = pfd->pfd_prefix[j];
		}

		if (pfc == NULL)
			continue;

		*f++ = '%';

		if (pfd->pfd_flags & DT_PFCONV_ALT)
			*f++ = '#';
		if (pfd->pfd_flags & DT_PFCONV_ZPAD)
			*f++ = '0';
		if (pfd->pfd_flags & DT_PFCONV_LEFT)
			*f++ = '-';
		if (pfd->pfd_flags & DT_PFCONV_SPOS)
			*f++ = '+';
		if (pfd->pfd_flags & DT_PFCONV_DYNWIDTH)
			*f++ = '*';
		if (pfd->pfd_flags & DT_PFCONV_DYNPREC) {
			*f++ = '.';
			*f++ = '*';
		}
		if (pfd->pfd_flags & DT_PFCONV_GROUP)
			*f++ = '\'';
		if (pfd->pfd_flags & DT_PFCONV_SPACE)
			*f++ = ' ';
		if (pfd->pfd_flags & DT_PFCONV_AGG)
			*f++ = '@';

		if (width != 0)
			f += snprintf(f, sizeof (format), "%d", width);

		if (prec != 0)
			f += snprintf(f, sizeof (format), ".%d", prec);

		/*
		 * If the output format is %s, then either %s is the underlying
		 * conversion or the conversion is one of our customized ones,
		 * e.g. pfprint_addr.  In these cases, put the original string
		 * name of the conversion (pfc_name) into the pickled format
		 * string rather than the derived conversion (pfd_fmt).
		 */
		if (strcmp(pfc->pfc_ofmt, "s") == 0)
			str = pfc->pfc_name;
		else
			str = pfd->pfd_fmt;

		for (j = 0; str[j] != '\0'; j++)
			*f++ = str[j];
	}

	*f = '\0'; /* insert nul byte; do not count in return value */

	assert(f < format + formatlen);
	(void) strncpy(s, format, len);

	return ((size_t)(f - format));
}

static int
dt_fprinta(dtrace_aggdata_t *adp, void *arg)
{
	dtrace_aggdesc_t *agg = adp->dtada_desc;
	const dtrace_recdesc_t *recp = &agg->dtagd_rec[0];
	uint_t nrecs = agg->dtagd_nrecs;
	dt_pfwalk_t *pfw = arg;
	int id;

	if (dt_printf_getint(pfw->pfw_dtp, recp++, nrecs--,
	    adp->dtada_data, adp->dtada_size, &id) != 0 || pfw->pfw_aid != id)
		return (0); /* no aggregation id or id does not match */

	if (dt_printf_format(pfw->pfw_dtp, pfw->pfw_fp, pfw->pfw_argv,
	    recp, nrecs, adp->dtada_data, adp->dtada_size,
	    adp->dtada_normal) == -1)
		return (pfw->pfw_err = pfw->pfw_dtp->dt_errno);

	agg->dtagd_flags |= DTRACE_AGD_PRINTED;

	if (dt_buffered_flush(pfw->pfw_dtp, NULL, &agg->dtagd_rec[0], adp) < 0)
		return (-1);

	return (0);
}

int
dtrace_fprinta(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
    const dtrace_recdesc_t *recs, uint_t nrecs, const void *buf, size_t len)
{
	const dtrace_recdesc_t *recp = recs;
	dt_pfwalk_t pfw;
	int id;

	if (dt_printf_getint(dtp, recp++, nrecs--, buf, len, &id) == -1)
		return (-1); /* errno is set for us */

	pfw.pfw_dtp = dtp;
	pfw.pfw_argv = fmtdata;
	pfw.pfw_aid = id;
	pfw.pfw_fp = fp;
	pfw.pfw_err = 0;

	if (dtrace_aggregate_walk_valsorted(dtp, dt_fprinta, &pfw) == -1 ||
	    pfw.pfw_err != 0)
		return (-1); /* errno is set for us */

	return ((int)(recp - recs));
}
