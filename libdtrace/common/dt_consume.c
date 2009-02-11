/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)dt_consume.c	1.15	04/12/18 SMI"

#include <linux_types.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <alloca.h>

#include <dt_impl.h>

static int
dt_flowindent(dtrace_hdl_t *dtp, dtrace_probedata_t *data, dtrace_epid_t last,
    dtrace_bufdesc_t *buf, size_t offs)
{
	dtrace_probedesc_t *pd = data->dtpda_pdesc, *npd;
	dtrace_eprobedesc_t *epd = data->dtpda_edesc, *nepd;
	char *p = pd->dtpd_provider, *n = pd->dtpd_name;
	dtrace_flowkind_t flow = DTRACEFLOW_NONE;
	const char *str = NULL;
	static const char *e_str[2] = { " -> ", " => " };
	static const char *r_str[2] = { " <- ", " <= " };
	dtrace_epid_t next, id = epd->dtepd_epid;
	int rval;

	if (strcmp(n, "entry") == 0) {
		flow = DTRACEFLOW_ENTRY;
		str = e_str[strcmp(p, "syscall") == 0];
	} else if (strcmp(n, "return") == 0 ||
	    strcmp(n, "exit") == 0) {
		flow = DTRACEFLOW_RETURN;
		str = r_str[strcmp(p, "syscall") == 0];
	}

	/*
	 * If we're going to indent this, we need to check the ID of our last
	 * call.  If we're looking at the same probe ID but a different EPID,
	 * we _don't_ want to indent.  (Yes, there are some minor holes in
	 * this scheme -- it's a heuristic.)
	 */
	if (flow == DTRACEFLOW_ENTRY) {
		if ((last != DTRACE_EPIDNONE && id != last &&
		    pd->dtpd_id == dtp->dt_pdesc[last]->dtpd_id))
			flow = DTRACEFLOW_NONE;
	}

	/*
	 * If we're going to unindent this, it's more difficult to see if
	 * we don't actually want to unindent it -- we need to look at the
	 * _next_ EPID.
	 */
	if (flow == DTRACEFLOW_RETURN) {
		offs += epd->dtepd_size;

		do {
			if (offs >= buf->dtbd_size) {
				/*
				 * We're at the end -- maybe.  If the oldest
				 * record is non-zero, we need to wrap.
				 */
				if (buf->dtbd_oldest != 0) {
					offs = 0;
				} else {
					goto out;
				}
			}

			next = *(uint32_t *)((uintptr_t)buf->dtbd_data + offs);

			if (next == DTRACE_EPIDNONE)
				offs += sizeof (id);
		} while (next == DTRACE_EPIDNONE);

		if ((rval = dt_epid_lookup(dtp, next, &nepd, &npd)) != 0)
			return (rval);

		if (next != id && npd->dtpd_id == pd->dtpd_id)
			flow = DTRACEFLOW_NONE;
	}

out:
	if (flow == DTRACEFLOW_ENTRY || flow == DTRACEFLOW_RETURN) {
		data->dtpda_prefix = str;
	} else {
		data->dtpda_prefix = "| ";
	}

	if (flow == DTRACEFLOW_RETURN && data->dtpda_indent > 0)
		data->dtpda_indent -= 2;

	data->dtpda_flow = flow;

	return (0);
}

static int
dt_nullprobe()
{
	return (DTRACE_CONSUME_THIS);
}

static int
dt_nullrec()
{
	return (DTRACE_CONSUME_NEXT);
}

int
dt_print_quantize(dtrace_hdl_t *dtp, FILE *fp, const void *addr,
    size_t size, uint64_t normal)
{
	const uint64_t *data = addr;
	int i, first_bin = 0, last_bin = DTRACE_QUANTIZE_NBUCKETS - 1;
	uint64_t total_bin_count = 0;

	if (size != DTRACE_QUANTIZE_NBUCKETS * sizeof (uint64_t))
		return (dt_set_errno(dtp, EDT_DMISMATCH));

	while (first_bin < DTRACE_QUANTIZE_NBUCKETS - 1 && data[first_bin] == 0)
		first_bin++;

	if (first_bin > 0)
		first_bin--;

	while (last_bin > 0 && data[last_bin] == 0)
		last_bin--;

	if (last_bin < DTRACE_QUANTIZE_NBUCKETS - 1)
		last_bin++;

	for (i = first_bin; i <= last_bin; i++)
		total_bin_count += data[i];

	if (dt_printf(dtp, fp, "\n%16s %41s %-9s\n", "value",
	    "------------- Distribution -------------", "count") < 0)
		return (-1);

	for (i = first_bin; i <= last_bin; i++) {
		float f = ((float)data[i] * 40.0) / (float)total_bin_count;
		uint_t depth = (uint_t)(f + 0.5);

		if (dt_printf(dtp, fp, "%16lld |%s%s %-9llu\n",
		    (long long)DTRACE_QUANTIZE_BUCKETVAL(i),
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" + 40 - depth,
		    "                                        " + depth,
		    (u_longlong_t)data[i] / normal) < 0)
			return (-1);
	}

	return (0);
}

int
dt_print_lquantize(dtrace_hdl_t *dtp, FILE *fp, const void *addr,
    size_t size, uint64_t normal)
{
	const uint64_t *data = addr;
	int i, first_bin, last_bin, base;
	uint64_t arg, total_bin_count = 0;
	uint16_t step, levels;

	if (size < sizeof (uint64_t))
		return (dt_set_errno(dtp, EDT_DMISMATCH));

	arg = *data++;
	size -= sizeof (uint64_t);

	base = DTRACE_LQUANTIZE_BASE(arg);
	step = DTRACE_LQUANTIZE_STEP(arg);
	levels = DTRACE_LQUANTIZE_LEVELS(arg);

	first_bin = 0;
	last_bin = levels + 1;

	if (size != sizeof (uint64_t) * (levels + 2))
		return (dt_set_errno(dtp, EDT_DMISMATCH));

	while (first_bin < levels + 1 && data[first_bin] == 0)
		first_bin++;

	if (first_bin > 0)
		first_bin--;

	while (last_bin > 0 && data[last_bin] == 0)
		last_bin--;

	if (last_bin < levels + 1)
		last_bin++;

	for (i = first_bin; i <= last_bin; i++)
		total_bin_count += data[i];

	if (dt_printf(dtp, fp, "\n%16s %41s %-9s\n", "value",
	    "------------- Distribution -------------", "count") < 0)
		return (-1);

	for (i = first_bin; i <= last_bin; i++) {
		float f = ((float)data[i] * 40.0) / (float)total_bin_count;
		uint_t depth = (uint_t)(f + 0.5);
		char c[32];
		int err;

		if (i == 0) {
			(void) snprintf(c, sizeof (c), "< %d",
			    base / (uint32_t)normal);
			err = dt_printf(dtp, fp, "%16s ", c);
		} else if (i == levels + 1) {
			(void) snprintf(c, sizeof (c), ">= %d",
			    base + (levels * step));
			err = dt_printf(dtp, fp, "%16s ", c);
		} else {
			err = dt_printf(dtp, fp, "%16d ",
			    base + (i - 1) * step);
		}

		if (err < 0 || dt_printf(dtp, fp, "|%s%s %-9llu\n",
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" + 40 - depth,
		    "                                        " + depth,
		    (u_longlong_t)data[i] / normal) < 0)
			return (-1);
	}

	return (0);
}

/*ARGSUSED*/
static int
dt_print_average(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr,
    size_t size, uint64_t normal)
{
	/* LINTED - alignment */
	uint64_t *data = (uint64_t *)addr;

	return (dt_printf(dtp, fp, " %16lld", data[0] ?
	    (long long)(data[1] / normal / data[0]) : 0));
}

/*ARGSUSED*/
int
dt_print_bytes(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr,
    size_t nbytes, int width, int quiet)
{
	/*
	 * If the byte stream is a series of printable characters, followed by
	 * a terminating byte, we print it out as a string.  Otherwise, we
	 * assume that it's something else and just print the bytes.
	 */
	int i, j, margin = 5;
	char *c = (char *)addr;

	if (nbytes == 0)
		return (0);

	if (dtp->dt_options[DTRACEOPT_RAWBYTES] != DTRACEOPT_UNSET)
		goto raw;

	for (i = 0; i < nbytes; i++) {
		/*
		 * We define a "printable character" to be one for which
		 * isprint(3C) returns non-zero, isspace(3C) returns non-zero,
		 * or a character which is either backspace or the bell.
		 * Backspace and the bell are regrettably special because
		 * they fail the first two tests -- and yet they are entirely
		 * printable.  These are the only two control characters that
		 * have meaning for the terminal and for which isprint(3C) and
		 * isspace(3C) return 0.
		 */
		if (isprint(c[i]) || isspace(c[i]) ||
		    c[i] == '\b' || c[i] == '\a')
			continue;

		if (c[i] == '\0' && i > 0) {
			/*
			 * This looks like it might be a string.  Before we
			 * assume that it is indeed a string, check the
			 * remainder of the byte range; if it contains
			 * additional non-nul characters, we'll assume that
			 * it's a binary stream that just happens to look like
			 * a string, and we'll print out the individual bytes.
			 */
			for (j = i + 1; j < nbytes; j++) {
				if (c[j] != '\0')
					break;
			}

			if (j != nbytes)
				break;

			if (quiet)
				return (dt_printf(dtp, fp, "%s", c));
			else
				return (dt_printf(dtp, fp, "  %-*s", width, c));
		}

		break;
	}

	if (i == nbytes) {
		/*
		 * The byte range is all printable characters, but there is
		 * no trailing nul byte.  We'll assume that it's a string and
		 * print it as such.
		 */
		char *s = alloca(nbytes + 1);
		bcopy(c, s, nbytes);
		s[nbytes] = '\0';
		return (dt_printf(dtp, fp, "  %-*s", width, s));
	}

raw:
	if (dt_printf(dtp, fp, "\n%*s      ", margin, "") < 0)
		return (-1);

	for (i = 0; i < 16; i++)
		if (dt_printf(dtp, fp, "  %c", "0123456789abcdef"[i]) < 0)
			return (-1);

	if (dt_printf(dtp, fp, "  0123456789abcdef\n") < 0)
		return (-1);


	for (i = 0; i < nbytes; i += 16) {
		if (dt_printf(dtp, fp, "%*s%5x:", margin, "", i) < 0)
			return (-1);

		for (j = i; j < i + 16 && j < nbytes; j++) {
			if (dt_printf(dtp, fp, " %02x", (uchar_t)c[j]) < 0)
				return (-1);
		}

		while (j++ % 16) {
			if (dt_printf(dtp, fp, "   ") < 0)
				return (-1);
		}

		if (dt_printf(dtp, fp, "  ") < 0)
			return (-1);

		for (j = i; j < i + 16 && j < nbytes; j++) {
			if (dt_printf(dtp, fp, "%c",
			    c[j] < ' ' || c[j] > '~' ? '.' : c[j]) < 0)
				return (-1);
		}

		if (dt_printf(dtp, fp, "\n") < 0)
			return (-1);
	}

	return (0);
}

int
dt_print_stack(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    caddr_t addr, int depth)
{
	pc_t *pc = (pc_t *)(uintptr_t)addr;
	dtrace_syminfo_t dts;
	GElf_Sym sym;
	int i, indent;
	char c[PATH_MAX * 2];

	if (dt_printf(dtp, fp, "\n") < 0)
		return (-1);

	if (format == NULL)
		format = "%s";

	if (dtp->dt_options[DTRACEOPT_STACKINDENT] != DTRACEOPT_UNSET)
		indent = (int)dtp->dt_options[DTRACEOPT_STACKINDENT];
	else
		indent = _dtrace_stkindent;

	for (i = 0; i < depth && pc[i] != NULL; i++) {
		if (dt_printf(dtp, fp, "%*s", indent, "") < 0)
			return (-1);

		if (dtrace_lookup_by_addr(dtp, pc[i], &sym, &dts) == 0) {
			if (pc[i] > sym.st_value) {
				(void) snprintf(c, sizeof (c), "%s`%s+0x%llx",
				    dts.dts_object, dts.dts_name,
				    (u_longlong_t)pc[i] - sym.st_value);
			} else {
				(void) snprintf(c, sizeof (c), "%s`%s",
				    dts.dts_object, dts.dts_name);
			}
		} else {
			/*
			 * We'll repeat the lookup, but this time we'll specify
			 * a NULL GElf_Sym -- indicating that we're only
			 * interested in the containing module.
			 */
			if (dtrace_lookup_by_addr(dtp, pc[i],
			    NULL, &dts) == 0) {
				(void) snprintf(c, sizeof (c), "%s`0x%llx",
				    dts.dts_object, (u_longlong_t)pc[i]);
			} else {
				(void) snprintf(c, sizeof (c), "0x%llx",
				    (u_longlong_t)pc[i]);
			}
		}

		if (dt_printf(dtp, fp, format, c) < 0)
			return (-1);

		if (dt_printf(dtp, fp, "\n") < 0)
			return (-1);
	}

	return (0);
}

int
dt_print_ustack(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    caddr_t addr, uint64_t arg)
{
	uint64_t *pc = (uint64_t *)(uintptr_t)addr;
	uint32_t depth = DTRACE_USTACK_NFRAMES(arg);
	uint32_t strsize = DTRACE_USTACK_STRSIZE(arg);
	const char *strbase = addr + (depth + 1) * sizeof (uint64_t);
	const char *str = strsize ? strbase : NULL;
	int err = 0;

	char name[PATH_MAX], objname[PATH_MAX], c[PATH_MAX * 2];
	struct ps_prochandle *P;
	GElf_Sym sym;
	int i, indent;
	pid_t pid;

	if (depth == 0)
		return (0);

	pid = (pid_t)*pc++;

	if (dt_printf(dtp, fp, "\n") < 0)
		return (-1);

	if (format == NULL)
		format = "%s";

	if (dtp->dt_options[DTRACEOPT_STACKINDENT] != DTRACEOPT_UNSET)
		indent = (int)dtp->dt_options[DTRACEOPT_STACKINDENT];
	else
		indent = _dtrace_stkindent;

	/*
	 * Ultimately, we need to add an entry point in the library vector for
	 * determining <symbol, offset> from <pid, address>.  For now, if
	 * this is a vector open, we just print the raw address or string.
	 */
	if (dtp->dt_vector == NULL)
		P = dt_proc_grab(dtp, pid, PGRAB_RDONLY | PGRAB_FORCE, 0);
	else
		P = NULL;

	if (P != NULL)
		dt_proc_lock(dtp, P); /* lock handle while we perform lookups */

	for (i = 0; i < depth && pc[i] != NULL; i++) {
		if ((err = dt_printf(dtp, fp, "%*s", indent, "")) < 0)
			break;

		if (P != NULL && Plookup_by_addr(P, pc[i],
		    name, sizeof (name), &sym) == 0) {
			(void) Pobjname(P, pc[i], objname, sizeof (objname));

			if (pc[i] > sym.st_value) {
				(void) snprintf(c, sizeof (c),
				    "%s`%s+0x%llx", dt_basename(objname), name,
				    (u_longlong_t)(pc[i] - sym.st_value));
			} else {
				(void) snprintf(c, sizeof (c),
				    "%s`%s", dt_basename(objname), name);
			}
		} else if (str != NULL && str[0] != '\0') {
			(void) snprintf(c, sizeof (c), "%s", str);
		} else {
			if (P != NULL && Pobjname(P, pc[i], objname,
			    sizeof (objname)) != NULL) {
				(void) snprintf(c, sizeof (c), "%s`0x%llx",
				    dt_basename(objname), (u_longlong_t)pc[i]);
			} else {
				(void) snprintf(c, sizeof (c), "0x%llx",
				    (u_longlong_t)pc[i]);
			}
		}

		if (err < 0)
			break;

		if (dt_printf(dtp, fp, format, c) < 0)
			return (-1);

		if (dt_printf(dtp, fp, "\n") < 0)
			return (-1);

		if (str != NULL) {
			str += strlen(str) + 1;
			if (str - strbase >= strsize)
				str = NULL;
		}
	}

	if (P != NULL) {
		dt_proc_unlock(dtp, P);
		dt_proc_release(dtp, P);
	}

	return (err);
}

typedef struct dt_normal {
	dtrace_aggvarid_t dtnd_id;
	uint64_t dtnd_normal;
} dt_normal_t;

static int
dt_normalize_agg(dtrace_aggdata_t *aggdata, void *arg)
{
	dt_normal_t *normal = arg;
	dtrace_aggdesc_t *agg = aggdata->dtada_desc;
	dtrace_aggvarid_t id = normal->dtnd_id;
	uintptr_t data = (uintptr_t)aggdata->dtada_data;

	if (agg->dtagd_nrecs == 0)
		return (DTRACE_AGGWALK_NEXT);

	if (id != *(dtrace_aggvarid_t *)(data + agg->dtagd_rec[0].dtrd_offset))
		return (DTRACE_AGGWALK_NEXT);

	aggdata->dtada_normal = normal->dtnd_normal;
	return (DTRACE_AGGWALK_NORMALIZE);
}

static int
dt_normalize(dtrace_hdl_t *dtp, caddr_t base, dtrace_recdesc_t *rec)
{
	dt_normal_t normal;
	caddr_t addr;

	/*
	 * We (should) have two records:  the aggregation ID followed by the
	 * normalization value.
	 */
	addr = base + rec->dtrd_offset;

	if (rec->dtrd_size != sizeof (dtrace_aggvarid_t))
		return (dt_set_errno(dtp, EDT_BADNORMAL));

	/* LINTED - alignment */
	normal.dtnd_id = *((dtrace_aggvarid_t *)addr);
	rec++;

	if (rec->dtrd_action != DTRACEACT_LIBACT)
		return (dt_set_errno(dtp, EDT_BADNORMAL));

	if (rec->dtrd_arg != DT_ACT_NORMALIZE)
		return (dt_set_errno(dtp, EDT_BADNORMAL));

	addr = base + rec->dtrd_offset;

	switch (rec->dtrd_size) {
	case sizeof (uint64_t):
		/* LINTED - alignment */
		normal.dtnd_normal = *((uint64_t *)addr);
		break;
	case sizeof (uint32_t):
		/* LINTED - alignment */
		normal.dtnd_normal = *((uint32_t *)addr);
		break;
	case sizeof (uint16_t):
		/* LINTED - alignment */
		normal.dtnd_normal = *((uint16_t *)addr);
		break;
	case sizeof (uint8_t):
		normal.dtnd_normal = *((uint8_t *)addr);
		break;
	default:
		return (dt_set_errno(dtp, EDT_BADNORMAL));
	}

	(void) dtrace_aggregate_walk(dtp, dt_normalize_agg, &normal);

	return (0);
}

static int
dt_denormalize_agg(dtrace_aggdata_t *aggdata, void *arg)
{
	dtrace_aggdesc_t *agg = aggdata->dtada_desc;
	dtrace_aggvarid_t id = *((dtrace_aggvarid_t *)arg);
	uintptr_t data = (uintptr_t)aggdata->dtada_data;

	if (agg->dtagd_nrecs == 0)
		return (DTRACE_AGGWALK_NEXT);

	if (id != *(dtrace_aggvarid_t *)(data + agg->dtagd_rec[0].dtrd_offset))
		return (DTRACE_AGGWALK_NEXT);

	return (DTRACE_AGGWALK_DENORMALIZE);
}

static int
dt_clear_agg(dtrace_aggdata_t *aggdata, void *arg)
{
	dtrace_aggdesc_t *agg = aggdata->dtada_desc;
	dtrace_aggvarid_t id = *((dtrace_aggvarid_t *)arg);
	uintptr_t data = (uintptr_t)aggdata->dtada_data;

	if (agg->dtagd_nrecs == 0)
		return (DTRACE_AGGWALK_NEXT);

	if (id != *(dtrace_aggvarid_t *)(data + agg->dtagd_rec[0].dtrd_offset))
		return (DTRACE_AGGWALK_NEXT);

	return (DTRACE_AGGWALK_CLEAR);
}

typedef struct dt_trunc {
	dtrace_aggvarid_t dttd_id;
	uint64_t dttd_remaining;
} dt_trunc_t;

static int
dt_trunc_agg(dtrace_aggdata_t *aggdata, void *arg)
{
	dt_trunc_t *trunc = arg;
	dtrace_aggdesc_t *agg = aggdata->dtada_desc;
	dtrace_aggvarid_t id = trunc->dttd_id;
	uintptr_t data = (uintptr_t)aggdata->dtada_data;

	if (agg->dtagd_nrecs == 0)
		return (DTRACE_AGGWALK_NEXT);

	if (id != *(dtrace_aggvarid_t *)(data + agg->dtagd_rec[0].dtrd_offset))
		return (DTRACE_AGGWALK_NEXT);

	if (trunc->dttd_remaining == 0)
		return (DTRACE_AGGWALK_REMOVE);

	trunc->dttd_remaining--;
	return (DTRACE_AGGWALK_NEXT);
}

static int
dt_trunc(dtrace_hdl_t *dtp, caddr_t base, dtrace_recdesc_t *rec)
{
	dt_trunc_t trunc;
	caddr_t addr;
	int64_t remaining;
	int (*func)(dtrace_hdl_t *, dtrace_aggregate_f *, void *);

	/*
	 * We (should) have two records:  the aggregation ID followed by the
	 * number of aggregation entries after which the aggregation is to be
	 * truncated.
	 */
	addr = base + rec->dtrd_offset;

	if (rec->dtrd_size != sizeof (dtrace_aggvarid_t))
		return (dt_set_errno(dtp, EDT_BADTRUNC));

	/* LINTED - alignment */
	trunc.dttd_id = *((dtrace_aggvarid_t *)addr);
	rec++;

	if (rec->dtrd_action != DTRACEACT_LIBACT)
		return (dt_set_errno(dtp, EDT_BADTRUNC));

	if (rec->dtrd_arg != DT_ACT_TRUNC)
		return (dt_set_errno(dtp, EDT_BADTRUNC));

	addr = base + rec->dtrd_offset;

	switch (rec->dtrd_size) {
	case sizeof (uint64_t):
		/* LINTED - alignment */
		remaining = *((int64_t *)addr);
		break;
	case sizeof (uint32_t):
		/* LINTED - alignment */
		remaining = *((int32_t *)addr);
		break;
	case sizeof (uint16_t):
		/* LINTED - alignment */
		remaining = *((int16_t *)addr);
		break;
	case sizeof (uint8_t):
		remaining = *((int8_t *)addr);
		break;
	default:
		return (dt_set_errno(dtp, EDT_BADNORMAL));
	}

	if (remaining < 0) {
		func = dtrace_aggregate_walk_valsorted;
		remaining = -remaining;
	} else {
		func = dtrace_aggregate_walk_valrevsorted;
	}

	assert(remaining >= 0);
	trunc.dttd_remaining = remaining;

	(void) func(dtp, dt_trunc_agg, &trunc);

	return (0);
}

int
dt_print_agg(dtrace_aggdata_t *aggdata, void *arg)
{
	int i, err = 0;
	dt_print_aggdata_t *pd = arg;
	dtrace_aggdesc_t *agg = aggdata->dtada_desc;
	FILE *fp = pd->dtpa_fp;
	dtrace_hdl_t *dtp = pd->dtpa_dtp;
	dtrace_aggvarid_t aggvarid = pd->dtpa_id;
	uintptr_t data = (uintptr_t)aggdata->dtada_data;

	if (pd->dtpa_allunprint) {
		if (agg->dtagd_flags & DTRACE_AGD_PRINTED)
			return (0);
	} else {
		/*
		 * If we're not printing all unprinted aggregations, then the
		 * aggregation variable ID denotes a specific aggregation
		 * variable that we should print -- skip any other aggregations
		 * that we encounter.
		 */
		if (agg->dtagd_nrecs == 0)
			return (0);

		if (aggvarid != *(dtrace_aggvarid_t *)(data +
		    agg->dtagd_rec[0].dtrd_offset))
			return (0);
	}

	/*
	 * Iterate over each record description, printing the traced data,
	 * skipping the first datum (the tuple member created by the compiler).
	 */
	for (i = 1; err >= 0 && i < agg->dtagd_nrecs; i++) {
		dtrace_recdesc_t *rec = &agg->dtagd_rec[i];
		dtrace_actkind_t act = rec->dtrd_action;
		caddr_t addr = aggdata->dtada_data + rec->dtrd_offset;
		size_t size = rec->dtrd_size;
		uint64_t normal;

		normal = DTRACEACT_ISAGG(act) ? aggdata->dtada_normal : 1;

		if (act == DTRACEACT_STACK) {
			int depth = rec->dtrd_size / sizeof (pc_t);
			err = dt_print_stack(dtp, fp, NULL, addr, depth);
			goto nextrec;
		}

		if (act == DTRACEACT_USTACK || act == DTRACEACT_JSTACK) {
			err = dt_print_ustack(dtp, fp, NULL, addr,
			    rec->dtrd_arg);
			goto nextrec;
		}

		if (act == DTRACEAGG_QUANTIZE) {
			err = dt_print_quantize(dtp, fp, addr, size, normal);
			goto nextrec;
		}

		if (act == DTRACEAGG_LQUANTIZE) {
			err = dt_print_lquantize(dtp, fp, addr, size, normal);
			goto nextrec;
		}

		if (act == DTRACEAGG_AVG) {
			err = dt_print_average(dtp, fp, addr, size, normal);
			goto nextrec;
		}

		switch (size) {
		case sizeof (uint64_t):
			err = dt_printf(dtp, fp, " %16lld",
			    /* LINTED - alignment */
			    (long long)*((uint64_t *)addr) / normal);
			break;
		case sizeof (uint32_t):
			/* LINTED - alignment */
			err = dt_printf(dtp, fp, " %8d", *((uint32_t *)addr) /
			    (uint32_t)normal);
			break;
		case sizeof (uint16_t):
			/* LINTED - alignment */
			err = dt_printf(dtp, fp, " %5d", *((uint16_t *)addr) /
			    (uint32_t)normal);
			break;
		case sizeof (uint8_t):
			err = dt_printf(dtp, fp, " %3d", *((uint8_t *)addr) /
			    (uint32_t)normal);
			break;
		default:
			err = dt_print_bytes(dtp, fp, addr, size, 50, 0);
			break;
		}

nextrec:
		if (dt_buffered_flush(dtp, NULL, rec, aggdata) < 0)
			return (-1);
	}

	if (err >= 0)
		err = dt_printf(dtp, fp, "\n");

	if (dt_buffered_flush(dtp, NULL, NULL, aggdata) < 0)
		return (-1);

	if (!pd->dtpa_allunprint)
		agg->dtagd_flags |= DTRACE_AGD_PRINTED;

	return (err < 0 ? -1 : 0);
}

static int
dt_consume_cpu(dtrace_hdl_t *dtp, FILE *fp, int cpu, dtrace_bufdesc_t *buf,
    dtrace_consume_probe_f *efunc, dtrace_consume_rec_f *rfunc, void *arg)
{
	dtrace_epid_t id;
	size_t offs, start = buf->dtbd_oldest, end = buf->dtbd_size;
	int flow = (dtp->dt_options[DTRACEOPT_FLOWINDENT] != DTRACEOPT_UNSET);
	int quiet = (dtp->dt_options[DTRACEOPT_QUIET] != DTRACEOPT_UNSET);
	int rval, i, n;
	dtrace_epid_t last = DTRACE_EPIDNONE;
	dtrace_probedata_t data;
	uint64_t drops;
	caddr_t addr;

	bzero(&data, sizeof (data));
	data.dtpda_handle = dtp;
	data.dtpda_cpu = cpu;

again:
	for (offs = start; offs < end; ) {
		dtrace_eprobedesc_t *epd;

		/*
		 * We're guaranteed to have an ID.
		 */
		id = *(uint32_t *)((uintptr_t)buf->dtbd_data + offs);

		if (id == DTRACE_EPIDNONE) {
			/*
			 * This is filler to assure proper alignment of the
			 * next record; we simply ignore it.
			 */
			offs += sizeof (id);
			continue;
		}

		if ((rval = dt_epid_lookup(dtp, id, &data.dtpda_edesc,
		    &data.dtpda_pdesc)) != 0)
			return (rval);

		epd = data.dtpda_edesc;
		data.dtpda_data = buf->dtbd_data + offs;

		if (data.dtpda_edesc->dtepd_uarg != DT_ECB_DEFAULT) {
			rval = dt_handle(dtp, &data);

			if (rval == DTRACE_CONSUME_NEXT)
				goto nextepid;

			if (rval == DTRACE_CONSUME_ERROR)
				return (-1);
		}

		if (flow)
			(void) dt_flowindent(dtp, &data, last, buf, offs);

		rval = (*efunc)(&data, arg);

		if (flow) {
			if (data.dtpda_flow == DTRACEFLOW_ENTRY)
				data.dtpda_indent += 2;
		}

		if (rval == DTRACE_CONSUME_NEXT)
			goto nextepid;

		if (rval == DTRACE_CONSUME_ABORT)
			return (dt_set_errno(dtp, EDT_DIRABORT));

		if (rval != DTRACE_CONSUME_THIS)
			return (dt_set_errno(dtp, EDT_BADRVAL));

		for (i = 0; i < epd->dtepd_nrecs; i++) {
			dtrace_recdesc_t *rec = &epd->dtepd_rec[i];
			dtrace_actkind_t act = rec->dtrd_action;

			data.dtpda_data = buf->dtbd_data + offs +
			    rec->dtrd_offset;
			addr = data.dtpda_data;

			if (act == DTRACEACT_LIBACT) {
				if (rec->dtrd_arg == DT_ACT_CLEAR) {
					dtrace_aggvarid_t id;

					/* LINTED - alignment */
					id = *((dtrace_aggvarid_t *)addr);
					(void) dtrace_aggregate_walk(dtp,
					    dt_clear_agg, &id);
					continue;
				}

				if (rec->dtrd_arg == DT_ACT_DENORMALIZE) {
					dtrace_aggvarid_t id;

					/* LINTED - alignment */
					id = *((dtrace_aggvarid_t *)addr);
					(void) dtrace_aggregate_walk(dtp,
					    dt_denormalize_agg, &id);
					continue;
				}

				if (rec->dtrd_arg == DT_ACT_NORMALIZE) {
					if (i == epd->dtepd_nrecs - 1)
						return (dt_set_errno(dtp,
						    EDT_BADNORMAL));

					if (dt_normalize(dtp,
					    buf->dtbd_data + offs, rec) != 0)
						return (-1);

					i++;
					continue;
				}

				if (rec->dtrd_arg == DT_ACT_TRUNC) {
					if (i == epd->dtepd_nrecs - 1)
						return (dt_set_errno(dtp,
						    EDT_BADTRUNC));

					if (dt_trunc(dtp,
					    buf->dtbd_data + offs, rec) != 0)
						return (-1);

					i++;
					continue;
				}

				if (rec->dtrd_arg == DT_ACT_FTRUNCATE) {
					if (fp == NULL)
						continue;

					(void) fflush(fp);
					(void) ftruncate(fileno(fp), 0);
					(void) fseeko(fp, 0, SEEK_SET);
					continue;
				}
			}

			rval = (*rfunc)(&data, rec, arg);

			if (rval == DTRACE_CONSUME_NEXT)
				continue;

			if (rval == DTRACE_CONSUME_ABORT)
				return (dt_set_errno(dtp, EDT_DIRABORT));

			if (rval != DTRACE_CONSUME_THIS)
				return (dt_set_errno(dtp, EDT_BADRVAL));

			if (act == DTRACEACT_STACK) {
				int depth = rec->dtrd_size / sizeof (pc_t);
				if (dt_print_stack(dtp, fp, NULL,
				    addr, depth) < 0)
					return (-1);
				goto nextrec;
			}

			if (act == DTRACEACT_USTACK ||
			    act == DTRACEACT_JSTACK) {
				if (dt_print_ustack(dtp, fp, NULL,
				    addr, rec->dtrd_arg) < 0)
					return (-1);
				goto nextrec;
			}

			if (DTRACEACT_ISPRINTFLIKE(act)) {
				void *fmtdata;
				int (*func)(dtrace_hdl_t *, FILE *, void *,
				    const dtrace_recdesc_t *, uint_t,
				    const void *buf, size_t);

				if ((fmtdata = dt_format_lookup(dtp,
				    rec->dtrd_format)) == NULL)
					goto nofmt;

				switch (act) {
				case DTRACEACT_PRINTF:
					func = dtrace_fprintf;
					break;
				case DTRACEACT_PRINTA:
					func = dtrace_fprinta;
					break;
				case DTRACEACT_SYSTEM:
					func = dtrace_system;
					break;
				}

				n = (*func)(dtp, fp, fmtdata,
				    rec, epd->dtepd_nrecs - i,
				    (uchar_t *)buf->dtbd_data + offs,
				    buf->dtbd_size - offs);

				if (n < 0)
					return (-1); /* errno is set for us */

				if (n > 0)
					i += n - 1;
				goto nextrec;
			}

nofmt:
			if (act == DTRACEACT_PRINTA) {
				dt_print_aggdata_t pd;

				bzero(&pd, sizeof (pd));
				pd.dtpa_dtp = dtp;
				pd.dtpa_fp = fp;
				/* LINTED - alignment */
				pd.dtpa_id = *((dtrace_aggvarid_t *)addr);

				if (dt_printf(dtp, fp, "\n") < 0 ||
				    dtrace_aggregate_walk_valsorted(dtp,
				    dt_print_agg, &pd) < 0)
					return (-1);

				goto nextrec;
			}

			switch (rec->dtrd_size) {
			case sizeof (uint64_t):
				n = dt_printf(dtp, fp,
				    quiet ? "%lld" : " %16lld",
				    /* LINTED - alignment */
				    *((unsigned long long *)addr));
				break;
			case sizeof (uint32_t):
				n = dt_printf(dtp, fp, quiet ? "%d" : " %8d",
				    /* LINTED - alignment */
				    *((uint32_t *)addr));
				break;
			case sizeof (uint16_t):
				n = dt_printf(dtp, fp, quiet ? "%d" : " %5d",
				    /* LINTED - alignment */
				    *((uint16_t *)addr));
				break;
			case sizeof (uint8_t):
				n = dt_printf(dtp, fp, quiet ? "%d" : " %3d",
				    *((uint8_t *)addr));
				break;
			default:
				n = dt_print_bytes(dtp, fp, addr,
				    rec->dtrd_size, 33, quiet);
				break;
			}

			if (n < 0)
				return (-1); /* errno is set for us */

nextrec:
			if (dt_buffered_flush(dtp, &data, rec, NULL) < 0)
				return (-1); /* errno is set for us */
		}

		/*
		 * Call the record callback with a NULL record to indicate
		 * that we're done processing this EPID.
		 */
		rval = (*rfunc)(&data, NULL, arg);
nextepid:
		offs += epd->dtepd_size;
		last = id;
	}

	if (buf->dtbd_oldest != 0 && start == buf->dtbd_oldest) {
		end = buf->dtbd_oldest;
		start = 0;
		goto again;
	}

	if ((drops = buf->dtbd_drops) == 0)
		return (0);

	/*
	 * Explicitly zero the drops to prevent us from processing them again.
	 */
	buf->dtbd_drops = 0;

	return (dt_handle_cpudrop(dtp, cpu, DTRACEDROP_PRINCIPAL, drops));
}

typedef struct dt_begin {
	dtrace_consume_probe_f *dtbgn_probefunc;
	void *dtbgn_arg;
	dtrace_handle_err_f *dtbgn_errhdlr;
	void *dtbgn_errarg;
	int dtbgn_beginonly;
} dt_begin_t;

static int
dt_consume_begin_probe(const dtrace_probedata_t *data, void *arg)
{
	dt_begin_t *begin = (dt_begin_t *)arg;
	dtrace_probedesc_t *pd = data->dtpda_pdesc;

	int r1 = (strcmp(pd->dtpd_provider, "dtrace") == 0);
	int r2 = (strcmp(pd->dtpd_name, "BEGIN") == 0);

	if (begin->dtbgn_beginonly) {
		if (!(r1 && r2))
			return (DTRACE_CONSUME_NEXT);
	} else {
		if (r1 && r2)
			return (DTRACE_CONSUME_NEXT);
	}

	/*
	 * We have a record that we're interested in.  Now call the underlying
	 * probe function...
	 */
	return (begin->dtbgn_probefunc(data, begin->dtbgn_arg));
}

static int
dt_consume_begin_error(dtrace_errdata_t *data, void *arg)
{
	dt_begin_t *begin = (dt_begin_t *)arg;
	dtrace_probedesc_t *pd = data->dteda_pdesc;

	int r1 = (strcmp(pd->dtpd_provider, "dtrace") == 0);
	int r2 = (strcmp(pd->dtpd_name, "BEGIN") == 0);

	if (begin->dtbgn_beginonly) {
		if (!(r1 && r2))
			return (DTRACE_HANDLE_OK);
	} else {
		if (r1 && r2)
			return (DTRACE_HANDLE_OK);
	}

	return (begin->dtbgn_errhdlr(data, begin->dtbgn_errarg));
}

static int
dt_consume_begin(dtrace_hdl_t *dtp, FILE *fp, dtrace_bufdesc_t *buf,
    dtrace_consume_probe_f *pf, dtrace_consume_rec_f *rf, void *arg)
{
	/*
	 * There's this idea that the BEGIN probe should be processed before
	 * everything else, and that the END probe should be processed after
	 * anything else.  In the common case, this is pretty easy to deal
	 * with.  However, a situation may arise where the BEGIN enabling and
	 * END enabling are on the same CPU, and some enabling in the middle
	 * occurred on a different CPU.  To deal with this (blech!) we need to
	 * consume the BEGIN buffer up until the end of the BEGIN probe, and
	 * then set it aside.  We will then process every other CPU, and then
	 * we'll return to the BEGIN CPU and process the rest of the data
	 * (which will inevitably include the END probe, if any).  Making this
	 * even more complicated (!) is the library's ERROR enabling.  Because
	 * this enabling is processed before we even get into the consume call
	 * back, any ERROR firing would result in the library's ERROR enabling
	 * being processed twice -- once in our first pass (for BEGIN probes),
	 * and again in our second pass (for everything but BEGIN probes).  To
	 * deal with this, we interpose on the ERROR handler to assure that we
	 * only process ERROR enablings induced by BEGIN enablings in the
	 * first pass, and that we only process ERROR enablings _not_ induced
	 * by BEGIN enablings in the second pass.
	 */
	dt_begin_t begin;
	processorid_t cpu = dtp->dt_beganon;
	dtrace_bufdesc_t nbuf;
	int rval, i;
	static int max_ncpus;
	dtrace_optval_t size;

	dtp->dt_beganon = -1;

	if (dt_ioctl(dtp, DTRACEIOC_BUFSNAP, buf) == -1) {
		/*
		 * We really don't expect this to fail, but it is at least
		 * technically possible for this to fail with ENOENT.  In this
		 * case, we just drive on...
		 */
		if (errno == ENOENT)
			return (0);

		return (dt_set_errno(dtp, errno));
	}

	if (!dtp->dt_stopped || buf->dtbd_cpu != dtp->dt_endedon) {
		/*
		 * This is the simple case.  We're either not stopped, or if
		 * we are, we actually processed any END probes on another
		 * CPU.  We can simply consume this buffer and return.
		 */
		return (dt_consume_cpu(dtp, fp, cpu, buf, pf, rf, arg));
	}

	begin.dtbgn_probefunc = pf;
	begin.dtbgn_arg = arg;
	begin.dtbgn_beginonly = 1;

	/*
	 * We need to interpose on the ERROR handler to be sure that we
	 * only process ERRORs induced by BEGIN.
	 */
	begin.dtbgn_errhdlr = dtp->dt_errhdlr;
	begin.dtbgn_errarg = dtp->dt_errarg;
	dtp->dt_errhdlr = dt_consume_begin_error;
	dtp->dt_errarg = &begin;

	rval = dt_consume_cpu(dtp,
	    fp, cpu, buf, dt_consume_begin_probe, rf, &begin);

	dtp->dt_errhdlr = begin.dtbgn_errhdlr;
	dtp->dt_errarg = begin.dtbgn_errarg;

	if (rval != 0)
		return (rval);

	/*
	 * Now allocate a new buffer.  We'll use this to deal with every other
	 * CPU.
	 */
	bzero(&nbuf, sizeof (dtrace_bufdesc_t));
	(void) dtrace_getopt(dtp, "bufsize", &size);
	if ((nbuf.dtbd_data = malloc(size)) == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	if (max_ncpus == 0)
		max_ncpus = dt_sysconf(dtp, _SC_CPUID_MAX) + 1;

	for (i = 0; i < max_ncpus; i++) {
		nbuf.dtbd_cpu = i;

		if (i == cpu)
			continue;

		if (dt_ioctl(dtp, DTRACEIOC_BUFSNAP, &nbuf) == -1) {
			/*
			 * If we failed with ENOENT, it may be because the
			 * CPU was unconfigured -- this is okay.  Any other
			 * error, however, is unexpected.
			 */
			if (errno == ENOENT)
				continue;

			free(nbuf.dtbd_data);

			return (dt_set_errno(dtp, errno));
		}

		if ((rval = dt_consume_cpu(dtp, fp,
		    i, &nbuf, pf, rf, arg)) != 0) {
			free(nbuf.dtbd_data);
			return (rval);
		}
	}

	free(nbuf.dtbd_data);

	/*
	 * Okay -- we're done with the other buffers.  Now we want to
	 * reconsume the first buffer -- but this time we're looking for
	 * everything _but_ BEGIN.  And of course, in order to only consume
	 * those ERRORs _not_ associatied with BEGIN, we need to reinstall our
	 * ERROR interposition function...
	 */
	begin.dtbgn_beginonly = 0;

	assert(begin.dtbgn_errhdlr == dtp->dt_errhdlr);
	assert(begin.dtbgn_errarg == dtp->dt_errarg);
	dtp->dt_errhdlr = dt_consume_begin_error;
	dtp->dt_errarg = &begin;

	rval = dt_consume_cpu(dtp,
	    fp, cpu, buf, dt_consume_begin_probe, rf, &begin);

	dtp->dt_errhdlr = begin.dtbgn_errhdlr;
	dtp->dt_errarg = begin.dtbgn_errarg;

	return (rval);
}

int
dtrace_consume(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_consume_probe_f *pf, dtrace_consume_rec_f *rf, void *arg)
{
	dtrace_bufdesc_t *buf = &dtp->dt_buf;
	dtrace_optval_t size;
	static int max_ncpus;
	int i, rval;
	dtrace_optval_t interval = dtp->dt_options[DTRACEOPT_SWITCHRATE];
	hrtime_t now = gethrtime();

	if (dtp->dt_lastswitch != 0) {
		if (now - dtp->dt_lastswitch < interval)
			return (0);

		dtp->dt_lastswitch += interval;
	} else {
		dtp->dt_lastswitch = now;
	}

	if (!dtp->dt_active)
		return (dt_set_errno(dtp, EINVAL));

	if (max_ncpus == 0)
		max_ncpus = dt_sysconf(dtp, _SC_CPUID_MAX) + 1;

	if (pf == NULL)
		pf = (dtrace_consume_probe_f *)dt_nullprobe;

	if (rf == NULL)
		rf = (dtrace_consume_rec_f *)dt_nullrec;

	if (buf->dtbd_data == NULL) {
		(void) dtrace_getopt(dtp, "bufsize", &size);
		if ((buf->dtbd_data = malloc(size)) == NULL)
			return (dt_set_errno(dtp, EDT_NOMEM));

		buf->dtbd_size = size;
	}

	/*
	 * If we have just begun, we want to first process the CPU that
	 * executed the BEGIN probe (if any).
	 */
	if (dtp->dt_active && dtp->dt_beganon != -1) {
		buf->dtbd_cpu = dtp->dt_beganon;
		if ((rval = dt_consume_begin(dtp, fp, buf, pf, rf, arg)) != 0)
			return (rval);
	}

	for (i = 0; i < max_ncpus; i++) {
		buf->dtbd_cpu = i;

		/*
		 * If we have stopped, we want to process the CPU on which the
		 * END probe was processed only _after_ we have processed
		 * everything else.
		 */
		if (dtp->dt_stopped && (i == dtp->dt_endedon))
			continue;

		if (dt_ioctl(dtp, DTRACEIOC_BUFSNAP, buf) == -1) {
			/*
			 * If we failed with ENOENT, it may be because the
			 * CPU was unconfigured -- this is okay.  Any other
			 * error, however, is unexpected.
			 */
			if (errno == ENOENT)
				continue;

			return (dt_set_errno(dtp, errno));
		}

		if ((rval = dt_consume_cpu(dtp, fp, i, buf, pf, rf, arg)) != 0)
			return (rval);
	}

	if (!dtp->dt_stopped)
		return (0);

	buf->dtbd_cpu = dtp->dt_endedon;

	if (dt_ioctl(dtp, DTRACEIOC_BUFSNAP, buf) == -1) {
		/*
		 * This _really_ shouldn't fail, but it is strictly speaking
		 * possible for this to return ENOENT if the CPU that called
		 * the END enabling somehow managed to become unconfigured.
		 * It's unclear how the user can possibly expect anything
		 * rational to happen in this case -- the state has been thrown
		 * out along with the unconfigured CPU -- so we'll just drive
		 * on...
		 */
		if (errno == ENOENT)
			return (0);

		return (dt_set_errno(dtp, errno));
	}

	return (dt_consume_cpu(dtp, fp, dtp->dt_endedon, buf, pf, rf, arg));
}
