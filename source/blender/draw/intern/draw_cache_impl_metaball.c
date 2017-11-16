/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_cache_impl_metaball.c
 *  \ingroup draw
 *
 * \brief MetaBall API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mball.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h"  /* own include */


static void metaball_batch_cache_clear(MetaBall *mb);

/* ---------------------------------------------------------------------- */
/* MetaBall Interface, indirect, partially cached access to complex data. */

typedef struct MetaBallRenderData {
	int types;

	/* borrow from 'Object' */
	CurveCache *ob_curve_cache;
} MetaBallRenderData;

enum {
	/* Geometry */
	MBALL_DATATYPE_SURFACE = 1 << 0,
//	MBALL_DATATYPE_WIRE    = 1 << 1,
//	MBALL_DATATYPE_SHADING = 1 << 2,
};

static MetaBallRenderData *metaball_render_data_create(MetaBall *mb, CurveCache *ob_curve_cache, const int types)
{
	MetaBallRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
	rdata->types = types;
	rdata->ob_curve_cache = ob_curve_cache;

/*
	**TODO**
	if (types & MBALL_DATATYPE_WIRE) {}
	if (types & MBALL_DATATYPE_SHADING) {}
*/

	return rdata;
}

static void metaball_render_data_free(MetaBallRenderData *rdata)
{
	MEM_freeN(rdata);
}

/* ---------------------------------------------------------------------- */
/* MetaBall Gwn_Batch Cache */

typedef struct MetaBallBatchCache {
	Gwn_Batch *batch;

	/* settings to determine if cache is invalid */
	bool is_dirty;
} MetaBallBatchCache;

/* Gwn_Batch cache management. */

static bool metaball_batch_cache_valid(MetaBall *mb)
{
	MetaBallBatchCache *cache = mb->batch_cache;

	if (cache == NULL) {
		return false;
	}

	return cache->is_dirty == false;
}

static void metaball_batch_cache_init(MetaBall *mb)
{
	MetaBallBatchCache *cache = mb->batch_cache;

	if (!cache) {
		cache = mb->batch_cache = MEM_mallocN(sizeof(*cache), __func__);
	}
	cache->batch = NULL;
	cache->is_dirty = false;
}

static MetaBallBatchCache *metaball_batch_cache_get(MetaBall *mb)
{
	if (!metaball_batch_cache_valid(mb)) {
		metaball_batch_cache_clear(mb);
		metaball_batch_cache_init(mb);
	}
	return mb->batch_cache;
}

void DRW_mball_batch_cache_dirty(MetaBall *mb, int mode)
{
	MetaBallBatchCache *cache = mb->batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_MBALL_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		default:
			BLI_assert(0);
	}
}

static void metaball_batch_cache_clear(MetaBall *mb)
{
	MetaBallBatchCache *cache = mb->batch_cache;
	if (!cache) {
		return;
	}

	GWN_BATCH_DISCARD_SAFE(cache->batch);
}

void DRW_mball_batch_cache_free(MetaBall *mb)
{
	metaball_batch_cache_clear(mb);
	MEM_SAFE_FREE(mb->batch_cache);
}

/* -------------------------------------------------------------------- */

/** \name Private MetaBall Cache API
 * \{ */

/* Gwn_Batch cache usage. */

static Gwn_Batch *metaball_batch_cache_get_pos_and_normals(MetaBallRenderData *rdata, MetaBallBatchCache *cache)
{
	BLI_assert(rdata->types & MBALL_DATATYPE_SURFACE);
	if (cache->batch == NULL) {
		cache->batch = BLI_displist_batch_calc_surface(&rdata->ob_curve_cache->disp);
	}
	return cache->batch;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Public Object/MetaBall API
 * \{ */

Gwn_Batch *DRW_metaball_batch_cache_get_triangles_with_normals(Object *ob)
{
	if (!BKE_mball_is_basis(ob))
		return NULL;

	MetaBall *mb = ob->data;
	MetaBallBatchCache *cache = metaball_batch_cache_get(mb);

	if (cache->batch == NULL) {
		MetaBallRenderData *rdata = metaball_render_data_create(mb, ob->curve_cache, MBALL_DATATYPE_SURFACE);
		metaball_batch_cache_get_pos_and_normals(rdata, cache);
		metaball_render_data_free(rdata);
	}

	return cache->batch;
}
