/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only.
 * See the file usr/src/LICENSING.NOTICE in this distribution or
 * http://www.opensolaris.org/license/ for details.
 */

#pragma ident	"@(#)dt_aggregate.c	1.6	04/12/18 SMI"

#include <linux_types.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <dt_impl.h>
#include <assert.h>

#define	DTRACE_AHASHSIZE	32779		/* big 'ol prime */

static void
dt_aggregate_count(uint64_t *existing, uint64_t *new, size_t size)
{
	int i;

	for (i = 0; i < size / sizeof (uint64_t); i++)
		existing[i] = existing[i] + new[i];
}

static int
dt_aggregate_countcmp(uint64_t *lhs, uint64_t *rhs)
{
	uint64_t lvar = *lhs;
	uint64_t rvar = *rhs;

	if (lvar > rvar)
		return (1);

	if (lvar < rvar)
		return (-1);

	return (0);
}

/*ARGSUSED*/
static void
dt_aggregate_min(uint64_t *existing, uint64_t *new, size_t size)
{
	if (*new < *existing)
		*existing = *new;
}

/*ARGSUSED*/
static void
dt_aggregate_max(uint64_t *existing, uint64_t *new, size_t size)
{
	if (*new > *existing)
		*existing = *new;
}

static int
dt_aggregate_averagecmp(uint64_t *lhs, uint64_t *rhs)
{
	uint64_t lavg = lhs[0] ? (lhs[1] / lhs[0]) : 0;
	uint64_t ravg = rhs[0] ? (rhs[1] / rhs[0]) : 0;

	if (lavg > ravg)
		return (1);

	if (lavg < ravg)
		return (-1);

	return (0);
}

/*ARGSUSED*/
static void
dt_aggregate_lquantize(uint64_t *existing, uint64_t *new, size_t size)
{
	uint64_t arg = *existing++;
	uint16_t levels = DTRACE_LQUANTIZE_LEVELS(arg);
	int i;

	for (i = 0; i <= levels + 1; i++)
		existing[i] = existing[i] + new[i + 1];
}

static int64_t
dt_aggregate_lquantizedsum(uint64_t *lquanta)
{
	uint64_t arg = *lquanta++;
	int32_t base = DTRACE_LQUANTIZE_BASE(arg);
	uint16_t step = DTRACE_LQUANTIZE_STEP(arg);
	uint16_t levels = DTRACE_LQUANTIZE_LEVELS(arg), i;
	int64_t total = lquanta[0] * (base - 1);

	for (i = 0; i < levels; base += step, i++)
		total += lquanta[i + 1] * base;

	return (total + lquanta[levels + 1] * (base + 1));
}

static int
dt_aggregate_lquantizedcmp(uint64_t *lhs, uint64_t *rhs)
{
	int64_t lsum = dt_aggregate_lquantizedsum(lhs);
	int64_t rsum = dt_aggregate_lquantizedsum(rhs);

	if (lsum > rsum)
		return (1);

	if (lsum < rsum)
		return (-1);

	return (0);
}

static int
dt_aggregate_quantizedcmp(uint64_t *lhs, uint64_t *rhs)
{
	int nbuckets = DTRACE_QUANTIZE_NBUCKETS, i;
	int64_t ltotal = 0, rtotal = 0;

	for (i = 0; i < nbuckets; i++) {
		int64_t bucketval = DTRACE_QUANTIZE_BUCKETVAL(i);

		ltotal += bucketval * lhs[i];
		rtotal += bucketval * rhs[i];
	}

	if (ltotal > rtotal)
		return (1);

	if (ltotal < rtotal)
		return (-1);

	return (0);
}

static int
dt_aggregate_snap_cpu(dtrace_hdl_t *dtp, processorid_t cpu)
{
	dtrace_epid_t id;
	uint64_t hashval;
	size_t offs, roffs, size, ndx;
	int i, j, rval;
	caddr_t addr, data;
	dtrace_recdesc_t *rec;
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	dtrace_aggdesc_t *agg;
	dt_ahash_t *hash = &agp->dtat_hash;
	dt_ahashent_t *h;
	dtrace_bufdesc_t b = agp->dtat_buf, *buf = &b;
	dtrace_aggdata_t *aggdata;
	int flags = agp->dtat_flags;

	buf->dtbd_cpu = cpu;

	if (dt_ioctl(dtp, DTRACEIOC_AGGSNAP, buf) == -1) {
		if (errno == ENOENT) {
			/*
			 * If that failed with ENOENT, it may be because the
			 * CPU was unconfigured.  This is okay; we'll just
			 * do nothing but return success.
			 */
			return (0);
		}

		return (dt_set_errno(dtp, errno));
	}

	if (buf->dtbd_drops != 0) {
		if (dt_handle_cpudrop(dtp, cpu,
		    DTRACEDROP_AGGREGATION, buf->dtbd_drops) == -1)
			return (-1);
	}

	if (buf->dtbd_size == 0)
		return (0);

	if (hash->dtah_hash == NULL) {
		size_t size;

		hash->dtah_size = DTRACE_AHASHSIZE;
		size = hash->dtah_size * sizeof (dt_ahashent_t *);

		if ((hash->dtah_hash = malloc(size)) == NULL)
			return (dt_set_errno(dtp, EDT_NOMEM));

		bzero(hash->dtah_hash, size);
	}

	for (offs = 0; offs < buf->dtbd_size; ) {
		/*
		 * We're guaranteed to have an ID.
		 */
		id = *((dtrace_epid_t *)((uintptr_t)buf->dtbd_data +
		    (uintptr_t)offs));

		if (id == DTRACE_AGGIDNONE) {
			/*
			 * This is filler to assure proper alignment of the
			 * next record; we simply ignore it.
			 */
			offs += sizeof (id);
			continue;
		}

		if ((rval = dt_aggid_lookup(dtp, id, &agg)) != 0)
			return (rval);

		addr = buf->dtbd_data + offs;
		size = agg->dtagd_size;
		hashval = 0;

		for (j = 0; j < agg->dtagd_nrecs - 1; j++) {
			rec = &agg->dtagd_rec[j];
			roffs = rec->dtrd_offset;

			for (i = 0; i < rec->dtrd_size; i++)
				hashval += addr[roffs + i];
		}

		ndx = hashval % hash->dtah_size;

		for (h = hash->dtah_hash[ndx]; h != NULL; h = h->dtahe_next) {
			if (h->dtahe_hashval != hashval)
				continue;

			if (h->dtahe_size != size)
				continue;

			aggdata = &h->dtahe_data;
			data = aggdata->dtada_data;

			for (j = 0; j < agg->dtagd_nrecs - 1; j++) {
				rec = &agg->dtagd_rec[j];
				roffs = rec->dtrd_offset;

				for (i = 0; i < rec->dtrd_size; i++)
					if (addr[roffs + i] != data[roffs + i])
						goto hashnext;
			}

			/*
			 * We found it.  Now we need to apply the aggregating
			 * action on the data here.
			 */
			rec = &agg->dtagd_rec[agg->dtagd_nrecs - 1];
			roffs = rec->dtrd_offset;
			/* LINTED - alignment */
			h->dtahe_aggregate((uint64_t *)&data[roffs],
			    /* LINTED - alignment */
			    (uint64_t *)&addr[roffs], rec->dtrd_size);

			/*
			 * If we're keeping per CPU data, apply the aggregating
			 * action there as well.
			 */
			if (aggdata->dtada_percpu != NULL) {
				data = aggdata->dtada_percpu[cpu];

				/* LINTED - alignment */
				h->dtahe_aggregate((uint64_t *)data,
				    /* LINTED - alignment */
				    (uint64_t *)&addr[roffs], rec->dtrd_size);
			}

			goto bufnext;
hashnext:
			continue;
		}

		/*
		 * If we're here, we couldn't find an entry for this record.
		 */
		if ((h = malloc(sizeof (dt_ahashent_t))) == NULL)
			return (dt_set_errno(dtp, EDT_NOMEM));
		bzero(h, sizeof (dt_ahashent_t));
		aggdata = &h->dtahe_data;

		if ((aggdata->dtada_data = malloc(size)) == NULL) {
			free(h);
			return (dt_set_errno(dtp, EDT_NOMEM));
		}

		bcopy(addr, aggdata->dtada_data, size);
		aggdata->dtada_size = size;
		aggdata->dtada_desc = agg;
		aggdata->dtada_handle = dtp;
		(void) dt_epid_lookup(dtp, agg->dtagd_epid,
		    &aggdata->dtada_edesc, &aggdata->dtada_pdesc);
		aggdata->dtada_normal = 1;

		h->dtahe_hashval = hashval;
		h->dtahe_size = size;

		rec = &agg->dtagd_rec[agg->dtagd_nrecs - 1];

		if (flags & DTRACE_A_PERCPU) {
			int max_cpus = agp->dtat_maxcpu;
			caddr_t *percpu = malloc(max_cpus * sizeof (caddr_t));

			if (percpu == NULL) {
				free(aggdata->dtada_data);
				free(h);
				return (dt_set_errno(dtp, EDT_NOMEM));
			}

			for (j = 0; j < max_cpus; j++) {
				percpu[j] = malloc(rec->dtrd_size);

				if (percpu[j] == NULL) {
					while (--j >= 0)
						free(percpu[j]);

					free(aggdata->dtada_data);
					free(h);
					return (dt_set_errno(dtp, EDT_NOMEM));
				}

				if (j == cpu) {
					bcopy(&addr[rec->dtrd_offset],
					    percpu[j], rec->dtrd_size);
				} else {
					bzero(percpu[j], rec->dtrd_size);
				}
			}

			aggdata->dtada_percpu = percpu;
		}

		switch (rec->dtrd_action) {
		case DTRACEAGG_MIN:
			h->dtahe_aggregate = dt_aggregate_min;
			break;

		case DTRACEAGG_MAX:
			h->dtahe_aggregate = dt_aggregate_max;
			break;

		case DTRACEAGG_LQUANTIZE:
			h->dtahe_aggregate = dt_aggregate_lquantize;
			break;

		case DTRACEAGG_COUNT:
		case DTRACEAGG_SUM:
		case DTRACEAGG_AVG:
		case DTRACEAGG_QUANTIZE:
			h->dtahe_aggregate = dt_aggregate_count;
			break;

		default:
			return (dt_set_errno(dtp, EDT_BADAGG));
		}

		if (hash->dtah_hash[ndx] != NULL)
			hash->dtah_hash[ndx]->dtahe_prev = h;

		h->dtahe_next = hash->dtah_hash[ndx];
		hash->dtah_hash[ndx] = h;

		if (hash->dtah_all != NULL)
			hash->dtah_all->dtahe_prevall = h;

		h->dtahe_nextall = hash->dtah_all;
		hash->dtah_all = h;
bufnext:
		offs += agg->dtagd_size;
	}

	return (0);
}

int
dtrace_aggregate_snap(dtrace_hdl_t *dtp)
{
	int i, rval;
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	hrtime_t now = gethrtime();
	dtrace_optval_t interval = dtp->dt_options[DTRACEOPT_AGGRATE];

	if (dtp->dt_lastagg != 0) {
		if (now - dtp->dt_lastagg < interval)
			return (0);

		dtp->dt_lastagg += interval;
	} else {
		dtp->dt_lastagg = now;
	}

	if (!dtp->dt_active)
		return (dt_set_errno(dtp, EINVAL));

	if (agp->dtat_buf.dtbd_size == 0)
		return (0);

	for (i = 0; i < agp->dtat_ncpus; i++) {
		if (rval = dt_aggregate_snap_cpu(dtp, agp->dtat_cpus[i]))
			return (rval);
	}

	return (0);
}

static int
dt_aggregate_hashcmp(const void *lhs, const void *rhs)
{
	dt_ahashent_t *lh = *((dt_ahashent_t **)lhs);
	dt_ahashent_t *rh = *((dt_ahashent_t **)rhs);
	dtrace_aggdesc_t *lagg = lh->dtahe_data.dtada_desc;
	dtrace_aggdesc_t *ragg = rh->dtahe_data.dtada_desc;

	if (lagg->dtagd_nrecs < ragg->dtagd_nrecs)
		return (-1);

	if (lagg->dtagd_nrecs > ragg->dtagd_nrecs)
		return (1);

	return (0);
}

static int
dt_aggregate_varcmp(const void *lhs, const void *rhs)
{
	dt_ahashent_t *lh = *((dt_ahashent_t **)lhs);
	dt_ahashent_t *rh = *((dt_ahashent_t **)rhs);
	dtrace_aggdesc_t *lagg = lh->dtahe_data.dtada_desc;
	dtrace_aggdesc_t *ragg = rh->dtahe_data.dtada_desc;
	caddr_t ldata = lh->dtahe_data.dtada_data;
	caddr_t rdata = rh->dtahe_data.dtada_data;
	dtrace_recdesc_t *lrec, *rrec;
	uint64_t lid, rid;

	/*
	 * We know that we have a compiler-generated ID as the first record.
	 */
	lrec = lagg->dtagd_rec;
	rrec = ragg->dtagd_rec;

	lid = *((uint64_t *)(uintptr_t)(ldata + lrec->dtrd_offset));
	rid = *((uint64_t *)(uintptr_t)(rdata + rrec->dtrd_offset));

	if (lid < rid)
		return (-1);

	if (lid > rid)
		return (1);

	return (0);
}

static int
dt_aggregate_valcmp(const void *lhs, const void *rhs)
{
	dt_ahashent_t *lh = *((dt_ahashent_t **)lhs);
	dt_ahashent_t *rh = *((dt_ahashent_t **)rhs);
	dtrace_aggdesc_t *lagg = lh->dtahe_data.dtada_desc;
	dtrace_aggdesc_t *ragg = rh->dtahe_data.dtada_desc;
	caddr_t ldata = lh->dtahe_data.dtada_data;
	caddr_t rdata = rh->dtahe_data.dtada_data;
	dtrace_recdesc_t *lrec, *rrec;
	uint64_t *laddr, *raddr;
	int rval;

	if ((rval = dt_aggregate_hashcmp(lhs, rhs)) != 0)
		return (rval);

	lrec = &lagg->dtagd_rec[lagg->dtagd_nrecs - 1];
	rrec = &ragg->dtagd_rec[ragg->dtagd_nrecs - 1];

	if (lrec->dtrd_offset < rrec->dtrd_offset)
		return (-1);

	if (lrec->dtrd_offset > rrec->dtrd_offset)
		return (1);

	if (lrec->dtrd_action < rrec->dtrd_action)
		return (-1);

	if (lrec->dtrd_action > rrec->dtrd_action)
		return (1);

	laddr = (uint64_t *)(uintptr_t)(ldata + lrec->dtrd_offset);
	raddr = (uint64_t *)(uintptr_t)(rdata + rrec->dtrd_offset);

	switch (lrec->dtrd_action) {
	case DTRACEAGG_AVG:
		return (dt_aggregate_averagecmp(laddr, raddr));

	case DTRACEAGG_QUANTIZE:
		return (dt_aggregate_quantizedcmp(laddr, raddr));

	case DTRACEAGG_LQUANTIZE:
		return (dt_aggregate_lquantizedcmp(laddr, raddr));

	case DTRACEAGG_COUNT:
	case DTRACEAGG_SUM:
	case DTRACEAGG_MIN:
	case DTRACEAGG_MAX:
		return (dt_aggregate_countcmp(laddr, raddr));

	default:
		assert(0);
	}

	return (0);
}

static int
dt_aggregate_keycmp(const void *lhs, const void *rhs)
{
	dt_ahashent_t *lh = *((dt_ahashent_t **)lhs);
	dt_ahashent_t *rh = *((dt_ahashent_t **)rhs);
	dtrace_aggdesc_t *lagg = lh->dtahe_data.dtada_desc;
	dtrace_aggdesc_t *ragg = rh->dtahe_data.dtada_desc;
	dtrace_recdesc_t *lrec, *rrec;
	char *ldata, *rdata;
	int rval, i, j;

	if ((rval = dt_aggregate_hashcmp(lhs, rhs)) != 0)
		return (rval);

	for (i = 1; i < lagg->dtagd_nrecs - 1; i++) {
		lrec = &lagg->dtagd_rec[i];
		rrec = &ragg->dtagd_rec[i];

		ldata = lh->dtahe_data.dtada_data + lrec->dtrd_offset;
		rdata = rh->dtahe_data.dtada_data + rrec->dtrd_offset;

		if (lrec->dtrd_size < rrec->dtrd_size)
			return (-1);

		if (lrec->dtrd_size > rrec->dtrd_size)
			return (1);

		for (j = 0; j < lrec->dtrd_size; j++) {
			char l = ldata[j];
			char r = rdata[j];

			if (l < r)
				return (-1);

			if (l > r)
				return (1);
		}
	}

	return (0);
}

static int
dt_aggregate_keyvarcmp(const void *lhs, const void *rhs)
{
	int rval;

	if ((rval = dt_aggregate_keycmp(lhs, rhs)) != 0)
		return (rval);

	return (dt_aggregate_varcmp(lhs, rhs));
}

static int
dt_aggregate_varkeycmp(const void *lhs, const void *rhs)
{
	int rval;

	if ((rval = dt_aggregate_varcmp(lhs, rhs)) != 0)
		return (rval);

	return (dt_aggregate_keycmp(lhs, rhs));
}

static int
dt_aggregate_valvarcmp(const void *lhs, const void *rhs)
{
	int rval;

	if ((rval = dt_aggregate_valcmp(lhs, rhs)) != 0)
		return (rval);

	return (dt_aggregate_varcmp(lhs, rhs));
}

static int
dt_aggregate_varvalcmp(const void *lhs, const void *rhs)
{
	int rval;

	if ((rval = dt_aggregate_varcmp(lhs, rhs)) != 0)
		return (rval);

	return (dt_aggregate_valcmp(lhs, rhs));
}

static int
dt_aggregate_keyvarrevcmp(const void *lhs, const void *rhs)
{
	return (dt_aggregate_keyvarcmp(rhs, lhs));
}

static int
dt_aggregate_varkeyrevcmp(const void *lhs, const void *rhs)
{
	return (dt_aggregate_varkeycmp(rhs, lhs));
}

static int
dt_aggregate_valvarrevcmp(const void *lhs, const void *rhs)
{
	return (dt_aggregate_valvarcmp(rhs, lhs));
}

static int
dt_aggregate_varvalrevcmp(const void *lhs, const void *rhs)
{
	return (dt_aggregate_varvalcmp(rhs, lhs));
}

int
dt_aggregate_go(dtrace_hdl_t *dtp)
{
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	dtrace_optval_t size, cpu;
	dtrace_bufdesc_t *buf = &agp->dtat_buf;
	int rval, i;

	assert(agp->dtat_maxcpu == 0);
	assert(agp->dtat_ncpu == 0);
	assert(agp->dtat_cpus == NULL);

	agp->dtat_maxcpu = dt_sysconf(dtp, _SC_CPUID_MAX) + 1;
	agp->dtat_ncpu = dt_sysconf(dtp, _SC_NPROCESSORS_MAX);
	agp->dtat_cpus = malloc(agp->dtat_ncpu * sizeof (processorid_t));

	if (agp->dtat_cpus == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	/*
	 * Use the aggregation buffer size as reloaded from the kernel.
	 */
	size = dtp->dt_options[DTRACEOPT_AGGSIZE];

	rval = dtrace_getopt(dtp, "aggsize", &size);
	assert(rval == 0);

	if (size == 0 || size == DTRACEOPT_UNSET)
		return (0);

	buf = &agp->dtat_buf;
	buf->dtbd_size = size;

	if ((buf->dtbd_data = malloc(buf->dtbd_size)) == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	/*
	 * Now query for the CPUs enabled.
	 */
	rval = dtrace_getopt(dtp, "cpu", &cpu);
	assert(rval == 0 && cpu != DTRACEOPT_UNSET);

	if (cpu != DTRACE_CPUALL) {
		assert(cpu < agp->dtat_ncpu);
		agp->dtat_cpus[agp->dtat_ncpus++] = (processorid_t)cpu;

		return (0);
	}

	agp->dtat_ncpus = 0;
	for (i = 0; i < agp->dtat_maxcpu; i++) {
		if (dt_status(dtp, i) == -1)
			continue;

		agp->dtat_cpus[agp->dtat_ncpus++] = i;
	}

	return (0);
}

static int
dt_aggwalk_rval(dtrace_hdl_t *dtp, dt_ahashent_t *h, int rval)
{
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	dtrace_aggdata_t *data;
	dtrace_aggdesc_t *aggdesc;
	dtrace_recdesc_t *rec;
	int i;

	switch (rval) {
	case DTRACE_AGGWALK_NEXT:
		break;

	case DTRACE_AGGWALK_CLEAR: {
		uint32_t size, offs = 0;

		aggdesc = h->dtahe_data.dtada_desc;
		rec = &aggdesc->dtagd_rec[aggdesc->dtagd_nrecs - 1];
		size = rec->dtrd_size;
		data = &h->dtahe_data;

		if (rec->dtrd_action == DTRACEAGG_LQUANTIZE) {
			offs = sizeof (uint64_t);
			size -= sizeof (uint64_t);
		}

		bzero(&data->dtada_data[rec->dtrd_offset] + offs, size);

		if (data->dtada_percpu == NULL)
			break;

		for (i = 0; i < dtp->dt_aggregate.dtat_maxcpu; i++)
			bzero(data->dtada_percpu[i] + offs, size);
		break;
	}

	case DTRACE_AGGWALK_ERROR:
		/*
		 * We assume that errno is already set in this case.
		 */
		return (dt_set_errno(dtp, errno));

	case DTRACE_AGGWALK_ABORT:
		return (dt_set_errno(dtp, EDT_DIRABORT));

	case DTRACE_AGGWALK_DENORMALIZE:
		h->dtahe_data.dtada_normal = 1;
		return (0);

	case DTRACE_AGGWALK_NORMALIZE:
		if (h->dtahe_data.dtada_normal == 0) {
			h->dtahe_data.dtada_normal = 1;
			return (dt_set_errno(dtp, EDT_BADRVAL));
		}

		return (0);

	case DTRACE_AGGWALK_REMOVE: {
		dtrace_aggdata_t *aggdata = &h->dtahe_data;
		int i, max_cpus = agp->dtat_maxcpu;

		/*
		 * First, remove this hash entry from its hash chain.
		 */
		if (h->dtahe_prev != NULL) {
			h->dtahe_prev->dtahe_next = h->dtahe_next;
		} else {
			dt_ahash_t *hash = &agp->dtat_hash;
			size_t ndx = h->dtahe_hashval % hash->dtah_size;

			assert(hash->dtah_hash[ndx] == h);
			hash->dtah_hash[ndx] = h->dtahe_next;
		}

		if (h->dtahe_next != NULL)
			h->dtahe_next->dtahe_prev = h->dtahe_prev;

		/*
		 * Now remove it from the list of all hash entries.
		 */
		if (h->dtahe_prevall != NULL) {
			h->dtahe_prevall->dtahe_nextall = h->dtahe_nextall;
		} else {
			dt_ahash_t *hash = &agp->dtat_hash;

			assert(hash->dtah_all == h);
			hash->dtah_all = h->dtahe_nextall;
		}

		if (h->dtahe_nextall != NULL)
			h->dtahe_nextall->dtahe_prevall = h->dtahe_prevall;

		/*
		 * We're unlinked.  We can safely destroy the data.
		 */
		if (aggdata->dtada_percpu != NULL) {
			for (i = 0; i < max_cpus; i++)
				free(aggdata->dtada_percpu[i]);
			free(aggdata->dtada_percpu);
		}

		free(aggdata->dtada_data);
		free(h);

		return (0);
	}

	default:
		return (dt_set_errno(dtp, EDT_BADRVAL));
	}

	return (0);
}

int
dtrace_aggregate_walk(dtrace_hdl_t *dtp, dtrace_aggregate_f *func, void *arg)
{
	dt_ahashent_t *h, *next;
	dt_ahash_t *hash = &dtp->dt_aggregate.dtat_hash;

	for (h = hash->dtah_all; h != NULL; h = next) {
		/*
		 * dt_aggwalk_rval() can potentially remove the current hash
		 * entry; we need to load the next hash entry before calling
		 * into it.
		 */
		next = h->dtahe_nextall;

		if (dt_aggwalk_rval(dtp, h, func(&h->dtahe_data, arg)) == -1)
			return (-1);
	}

	return (0);
}

static int
dt_aggregate_walk_sorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg,
    int (*sfunc)(const void *, const void *))
{
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	dt_ahashent_t *h, **sorted;
	dt_ahash_t *hash = &agp->dtat_hash;
	size_t i, nentries = 0;

	for (h = hash->dtah_all; h != NULL; h = h->dtahe_nextall)
		nentries++;

	sorted = malloc(nentries * sizeof (dt_ahashent_t *));

	if (sorted == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	for (h = hash->dtah_all, i = 0; h != NULL; h = h->dtahe_nextall)
		sorted[i++] = h;

	qsort(sorted, nentries, sizeof (dt_ahashent_t *), sfunc);

	for (i = 0; i < nentries; i++) {
		h = sorted[i];

		if (dt_aggwalk_rval(dtp, h, func(&h->dtahe_data, arg)) == -1)
			return (-1);
	}

	free(sorted);
	return (0);
}

int
dtrace_aggregate_walk_keysorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_varkeycmp));
}

int
dtrace_aggregate_walk_valsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_varvalcmp));
}

int
dtrace_aggregate_walk_keyvarsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_keyvarcmp));
}

int
dtrace_aggregate_walk_valvarsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_valvarcmp));
}

int
dtrace_aggregate_walk_keyrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_varkeyrevcmp));
}

int
dtrace_aggregate_walk_valrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_varvalrevcmp));
}

int
dtrace_aggregate_walk_keyvarrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_keyvarrevcmp));
}

int
dtrace_aggregate_walk_valvarrevsorted(dtrace_hdl_t *dtp,
    dtrace_aggregate_f *func, void *arg)
{
	return (dt_aggregate_walk_sorted(dtp, func,
	    arg, dt_aggregate_valvarrevcmp));
}

int
dtrace_aggregate_print(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_aggregate_walk_f *func)
{
	dt_print_aggdata_t pd;

	pd.dtpa_dtp = dtp;
	pd.dtpa_fp = fp;
	pd.dtpa_allunprint = 1;

	if (func == NULL)
		func = dtrace_aggregate_walk_valsorted;

	if ((*func)(dtp, dt_print_agg, &pd) == -1)
		return (dt_set_errno(dtp, dtp->dt_errno));

	return (0);
}

void
dtrace_aggregate_clear(dtrace_hdl_t *dtp)
{
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	dt_ahash_t *hash = &agp->dtat_hash;
	dt_ahashent_t *h;
	dtrace_aggdata_t *data;
	dtrace_aggdesc_t *aggdesc;
	dtrace_recdesc_t *rec;
	int i, max_cpus = agp->dtat_maxcpu;

	for (h = hash->dtah_all; h != NULL; h = h->dtahe_nextall) {
		aggdesc = h->dtahe_data.dtada_desc;
		rec = &aggdesc->dtagd_rec[aggdesc->dtagd_nrecs - 1];
		data = &h->dtahe_data;

		bzero(&data->dtada_data[rec->dtrd_offset], rec->dtrd_size);

		if (data->dtada_percpu == NULL)
			continue;

		for (i = 0; i < max_cpus; i++)
			bzero(data->dtada_percpu[i], rec->dtrd_size);
	}
}

void
dt_aggregate_destroy(dtrace_hdl_t *dtp)
{
	dt_aggregate_t *agp = &dtp->dt_aggregate;
	dt_ahash_t *hash = &agp->dtat_hash;
	dt_ahashent_t *h, *next;
	dtrace_aggdata_t *aggdata;
	int i, max_cpus = agp->dtat_maxcpu;

	if (hash->dtah_hash == NULL) {
		assert(hash->dtah_all == NULL);
	} else {
		free(hash->dtah_hash);

		for (h = hash->dtah_all; h != NULL; h = next) {
			next = h->dtahe_nextall;

			aggdata = &h->dtahe_data;

			if (aggdata->dtada_percpu != NULL) {
				for (i = 0; i < max_cpus; i++)
					free(aggdata->dtada_percpu[i]);
				free(aggdata->dtada_percpu);
			}

			free(aggdata->dtada_data);
			free(h);
		}

		hash->dtah_hash = NULL;
		hash->dtah_all = NULL;
		hash->dtah_size = 0;
	}

	free(agp->dtat_buf.dtbd_data);
	free(agp->dtat_cpus);
}
