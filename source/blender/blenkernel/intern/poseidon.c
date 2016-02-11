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
 * Contributor(s): Kevin Dietrich.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_poseidon_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_collision.h"
#include "BKE_DerivedMesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"
#include "BKE_poseidon.h"

#define DT_DEFAULT 0.1f
//#define DEBUG_TIME

#ifdef DEBUG_TIME
#	include "PIL_time.h"
#endif

#include "openvdb_capi.h"
#include "openvdb_mesh_capi.h"
#include "poseidon_capi.h"

typedef struct OpenVDBDerivedMeshIterator {
	OpenVDBMeshIterator base;
	struct MVert *vert, *vert_end;
	const struct MLoopTri *looptri, *looptri_end;
	struct MLoop *loop;
} OpenVDBDerivedMeshIterator;

static bool openvdb_dm_has_vertices(OpenVDBDerivedMeshIterator *it)
{
	return it->vert < it->vert_end;
}

static bool openvdb_dm_has_triangles(OpenVDBDerivedMeshIterator *it)
{
	return it->looptri < it->looptri_end;
}

static void openvdb_dm_next_vertex(OpenVDBDerivedMeshIterator *it)
{
	++it->vert;
}

static void openvdb_dm_next_triangle(OpenVDBDerivedMeshIterator *it)
{
	++it->looptri;
}

static void openvdb_dm_get_vertex(OpenVDBDerivedMeshIterator *it, float co[3])
{
	copy_v3_v3(co, it->vert->co);
}

static void openvdb_dm_get_triangle(OpenVDBDerivedMeshIterator *it, int *a, int *b, int *c)
{
	*a = it->loop[it->looptri->tri[0]].v;
	*b = it->loop[it->looptri->tri[1]].v;
	*c = it->loop[it->looptri->tri[2]].v;
}

static inline void openvdb_dm_iter_init(OpenVDBDerivedMeshIterator *it, DerivedMesh *dm)
{
	it->base.has_vertices = (OpenVDBMeshHasVerticesFn)openvdb_dm_has_vertices;
	it->base.has_triangles = (OpenVDBMeshHasTrianglesFn)openvdb_dm_has_triangles;
	it->base.next_vertex = (OpenVDBMeshNextVertexFn)openvdb_dm_next_vertex;
	it->base.next_triangle = (OpenVDBMeshNextTriangleFn)openvdb_dm_next_triangle;
	it->base.get_vertex = (OpenVDBMeshGetVertexFn)openvdb_dm_get_vertex;
	it->base.get_triangle = (OpenVDBMeshGetTriangleFn)openvdb_dm_get_triangle;

	it->vert = dm->getVertArray(dm);
	it->vert_end = it->vert + dm->getNumVerts(dm);
	it->looptri = dm->getLoopTriArray(dm);
	it->looptri_end = it->looptri + dm->getNumLoopTri(dm);
	it->loop = dm->getLoopArray(dm);
}

static void poseidon_domain_free(PoseidonDomainSettings *domain);
static void poseidon_flow_free(PoseidonFlowSettings *flow);
static void poseidon_collision_free(PoseidonCollSettings *coll);

void BKE_poseidon_modifier_create_type(PoseidonModifierData *pmd)
{
	if (!pmd) {
		return;
	}

	if (pmd->type & MOD_SMOKE_TYPE_DOMAIN) {
		if (pmd->domain) {
			poseidon_domain_free(pmd->domain);
		}

		pmd->domain = MEM_callocN(sizeof(PoseidonDomainSettings), "PoseidonDomainSettings");
		pmd->domain->pmd = pmd;

		pmd->domain->fluid_group = NULL;
		pmd->domain->coll_group = NULL;

		pmd->domain->cache = BKE_ptcache_add(&(pmd->domain->ptcaches));
		pmd->domain->cache->flag |= PTCACHE_DISK_CACHE;
		pmd->domain->cache->step = 1;

		pmd->domain->data = NULL;

		pmd->domain->voxel_size = 0.1f;
	}
	else if (pmd->type & MOD_SMOKE_TYPE_FLOW) {
		if (pmd->flow) {
			poseidon_flow_free(pmd->flow);
		}

		pmd->flow = MEM_callocN(sizeof(PoseidonFlowSettings), "PoseidonFlowSettings");
		pmd->flow->pmd = pmd;
	}
	else if (pmd->type & MOD_SMOKE_TYPE_COLL) {
		if (pmd->coll) {
			poseidon_collision_free(pmd->coll);
		}

		pmd->coll = MEM_callocN(sizeof(PoseidonCollSettings), "PoseidonCollSettings");
		pmd->coll->pmd = pmd;
	}
}

static void poseidon_domain_free(PoseidonDomainSettings *domain)
{
	if (!domain) {
		return;
	}

	BKE_ptcache_free_list(&domain->ptcaches);
	domain->cache = NULL;

	PoseidonData_free(domain->data);
	domain->data = NULL;

	if (domain->buffer) {
		MEM_freeN(domain->buffer);
		domain->buffer = NULL;
	}

	MEM_freeN(domain);
}

static void poseidon_flow_free(PoseidonFlowSettings *flow)
{
	if (!flow) {
		return;
	}

	if (flow->dm) {
		flow->dm->release(flow->dm);
		flow->dm = NULL;
	}

	if (flow->numverts && flow->verts_old) {
		MEM_freeN(flow->verts_old);
		flow->verts_old = NULL;
	}

	MEM_freeN(flow);
}

static void poseidon_collision_free(PoseidonCollSettings *coll)
{
	if (!coll) {
		return;
	}

	if (coll->dm) {
		coll->dm->release(coll->dm);
		coll->dm = NULL;
	}

	if (coll->numverts && coll->verts_old) {
		MEM_freeN(coll->verts_old);
		coll->verts_old = NULL;
	}

	MEM_freeN(coll);
}

void BKE_poseidon_modifier_free(PoseidonModifierData *pmd)
{
	if (!pmd) {
		return;
	}

	poseidon_domain_free(pmd->domain);
	pmd->domain = NULL;

	poseidon_flow_free(pmd->flow);
	pmd->flow = NULL;

	poseidon_collision_free(pmd->coll);
	pmd->coll = NULL;
}

static int poseidon_modifier_init(PoseidonModifierData *pmd, Object *ob, Scene *scene, DerivedMesh *dm)
{
	UNUSED_VARS(ob, dm);
	if ((pmd->type & MOD_SMOKE_TYPE_DOMAIN) && pmd->domain) {
		if (pmd->time != CFRA && pmd->domain->buffer) {
			printf("Freeing domain buffer!\n");
			MEM_freeN(pmd->domain->buffer);
			pmd->domain->buffer = NULL;
		}

		pmd->time = CFRA;

		/* initialize poseidon data */
		if (!pmd->domain->data) {
			pmd->domain->data = PoseidonData_create();
			PoseidonData_init(pmd->domain->data, pmd->domain->voxel_size, 0);
		}

		return true;
	}
	else if ((pmd->type & MOD_SMOKE_TYPE_FLOW) && pmd->flow) {
		pmd->time = CFRA;
		return true;
	}
	else if ((pmd->type & MOD_SMOKE_TYPE_COLL)) {
		if (!pmd->coll) {
			BKE_poseidon_modifier_create_type(pmd);
		}

		pmd->time = CFRA;
		return true;
	}

	return false;
}

void BKE_poseidon_modifier_reset(PoseidonModifierData *pmd)
{
	if (!pmd) {
		return;
	}

	if (pmd->domain) {
		pmd->time = -1;
	}
	else if (pmd->flow) {
		if (pmd->flow->verts_old) {
			MEM_freeN(pmd->flow->verts_old);
			pmd->flow->verts_old = NULL;
			pmd->flow->numverts = 0;
		}
	}
	else if (pmd->coll) {
		PoseidonCollSettings *coll = pmd->coll;

		if (coll->numverts && coll->verts_old) {
			MEM_freeN(coll->verts_old);
			coll->verts_old = NULL;
		}
	}
}

static void update_obstacles(Scene *scene, Object *ob, PoseidonDomainSettings *pds)
{
//	OpenVDB_smoke_clear_obstacles(sds->data);

	unsigned int numflowobj = 0;
	Object **collobjs = get_collisionobjects(scene, ob, pds->coll_group, &numflowobj, eModifierType_Poseidon);

	for (unsigned int flowIndex = 0; flowIndex < numflowobj; flowIndex++) {
		Object *collob = collobjs[flowIndex];
		PoseidonModifierData *smd2 = (PoseidonModifierData *)modifiers_findByType(collob, eModifierType_Poseidon);

		// check for initialized smoke object
		if ((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll) {
			PoseidonCollSettings *pcs = smd2->coll;

			float mat[4][4];
			mul_m4_m4m4(mat, pds->imat, collob->obmat);

			OpenVDBDerivedMeshIterator iter;
			openvdb_dm_iter_init(&iter, pcs->dm);

			struct OpenVDBPrimitive *prim = OpenVDBPrimitive_from_mesh(&iter.base, mat, pds->voxel_size);

			OpenVDBPrimitive_free(prim);
		}
	}

	if (collobjs) {
		MEM_freeN(collobjs);
	}
}

static void update_sources(Scene *scene, Object *ob, PoseidonDomainSettings *pds)
{
	unsigned int numflowobj = 0;
	Object **flowobjs = get_collisionobjects(scene, ob, pds->fluid_group, &numflowobj, eModifierType_Poseidon);

	for (unsigned int flowIndex = 0; flowIndex < numflowobj; flowIndex++) {
		Object *collob = flowobjs[flowIndex];
		PoseidonModifierData *smd2 = (PoseidonModifierData *)modifiers_findByType(collob, eModifierType_Poseidon);

		/* check for initialized smoke object */
		if ((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
			printf("%s\n", __func__);
			PoseidonFlowSettings *pfs = smd2->flow;

			float mat[4][4];
			mul_m4_m4m4(mat, pds->imat, collob->obmat);

			OpenVDBDerivedMeshIterator iter;
			openvdb_dm_iter_init(&iter, pfs->dm);

			struct OpenVDBPrimitive *prim = OpenVDBPrimitive_from_mesh(&iter.base, mat, pds->voxel_size);

			PoseidonData_add_inflow(pds->data, prim);

			OpenVDBPrimitive_free(prim);
		}
	}

	if (flowobjs) {
		MEM_freeN(flowobjs);
	}
}

static void step(Scene *scene, Object *ob, PoseidonModifierData *pmd, DerivedMesh *domain_dm)
{
	PoseidonDomainSettings *pds = pmd->domain;

	float gravity[3] = { 0.0f, 0.0f, -1.0f };

	/* update object state */
	invert_m4_m4(pds->imat, ob->obmat);
	copy_m4_m4(pds->obmat, ob->obmat);
//	smoke_set_domain_from_derivedmesh(sds, ob, domain_dm, (sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) != 0);

	/* use global gravity if enabled */
	if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(gravity, scene->physics_settings.gravity);
		/* map default value to 1.0 */
		mul_v3_fl(gravity, 1.0f / 9.810f);
	}

	/* convert gravity to domain space */
	const float gravity_mag = len_v3(gravity);
	mul_mat3_m4_v3(pds->imat, gravity);
	normalize_v3(gravity);
	mul_v3_fl(gravity, gravity_mag);

	/* adapt timestep for different framerates, dt = 0.1 is at 25fps */
	const float dt = DT_DEFAULT * (25.0f / FPS);

#if 0
	/* Disable substeps for now, since it results in numerical instability */

	// maximum timestep/"CFL" constraint: dt < 5.0 *dx / maxVel
	// maxVel should be 1.5 (1.5 cell max movement) * dx (cell size)
	const float maxVel = (pds->dx * 5.0f);

	const float maxVelMag = sqrtf(maxVelMag) * dt * pds->time_scale;

	const int maxSubSteps = 25;

	int totalSubsteps = (int)((maxVelMag / maxVel) + 1.0f); /* always round up */
	totalSubsteps = (totalSubsteps < 1) ? 1 : totalSubsteps;
	totalSubsteps = (totalSubsteps > maxSubSteps) ? maxSubSteps : totalSubsteps;

	const float dtSubdiv = (float)dt / (float)totalSubsteps;
	// printf("totalSubsteps: %d, maxVelMag: %f, dt: %f\n", totalSubsteps, maxVelMag, dt);
#else
	int totalSubsteps = 1;
	const float dtSubdiv = dt;
#endif

	for (int substep = 0; substep < totalSubsteps; substep++) {
		// calc animated obstacle velocities
		update_sources(scene, ob, pds);
		update_obstacles(scene, ob, pds);

		 // DG TODO? problem --> uses forces instead of velocity, need to check how they need to be changed with variable dt
		//update_effectors(scene, ob, pds, dtSubdiv);
		PoseidonData_step(pds->data, dtSubdiv);
	}
}

static void poseidon_modifier_process(PoseidonModifierData *pmd, Scene *scene,
                                      Object *ob, DerivedMesh *dm)
{
	if ((pmd->type & MOD_SMOKE_TYPE_FLOW)) {
		printf("%s: %s\n", __func__, "(pmd->type & MOD_SMOKE_TYPE_FLOW)");
		if (CFRA >= pmd->time) {
			poseidon_modifier_init(pmd, ob, scene, dm);
		}

		if (pmd->flow->dm) {
			pmd->flow->dm->release(pmd->flow->dm);
		}

		pmd->flow->dm = CDDM_copy(dm);
		DM_ensure_looptri(pmd->flow->dm);

		if (CFRA < pmd->time) {
			pmd->time = CFRA;
			BKE_poseidon_modifier_reset(pmd);
		}
	}
	else if (pmd->type & MOD_SMOKE_TYPE_COLL) {
		printf("%s: %s\n", __func__, "(pmd->type & MOD_SMOKE_TYPE_COLL)");
		if (CFRA >= pmd->time) {
			poseidon_modifier_init(pmd, ob, scene, dm);
		}

		if (pmd->coll) {
			if (pmd->coll->dm) {
				pmd->coll->dm->release(pmd->coll->dm);
			}

			pmd->coll->dm = CDDM_copy(dm);
			DM_ensure_looptri(pmd->coll->dm);
		}

		if (CFRA < pmd->time) {
			pmd->time = CFRA;
			BKE_poseidon_modifier_reset(pmd);
		}
	}
	else if (pmd->type & MOD_SMOKE_TYPE_DOMAIN) {
		printf("%s: %s\n", __func__, "(pmd->type & MOD_SMOKE_TYPE_DOMAIN)");
		PTCacheID pid;
		BKE_ptcache_id_from_poseidon(&pid, ob, pmd);

		int framenr = CFRA;
		int startframe, endframe;
		BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, NULL);

		printf("frame start: %d, end %d\n", startframe, endframe);

		PoseidonDomainSettings *pds = pmd->domain;
		PointCache *cache = pds->cache;
		if (!pds->data || framenr == startframe) {
			printf("%s: %s\n", __func__, "(!pds->data || framenr == startframe)");
			BKE_poseidon_modifier_reset(pmd);

			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_validate(cache, framenr);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}

		if (!pds->data && (framenr != startframe) && (pds->flags & MOD_POSEIDON_FILE_LOAD) == 0 && (cache->flag & PTCACHE_BAKED) == 0)
			return;

		pds->flags &= ~MOD_POSEIDON_FILE_LOAD;
		CLAMP(framenr, startframe, endframe);

		/* If already viewing a pre/after frame, no need to reload */
		if ((pmd->time == framenr) && (framenr != CFRA)) {
			printf("%s: %s\n", __func__, "(pmd->time == framenr) && (framenr != CFRA)");
			return;
		}

		if (!poseidon_modifier_init(pmd, ob, scene, dm)) {
			printf("Warning: bad Poseidon initialization!\n");
			return;
		}

		/* try to read from cache */
		if (BKE_ptcache_read(&pid, (float)framenr) == PTCACHE_READ_EXACT) {
			printf("%s: %s\n", __func__, "BKE_ptcache_read(&pid, (float)framenr) == PTCACHE_READ_EXACT");
			BKE_ptcache_validate(cache, framenr);
			pmd->time = framenr;
			return;
		}

		/* only calculate something when we advanced a single frame */
//		if (framenr != (int)pmd->time + 1) {
//			printf("%s: %s\n", __func__, "(framenr != (int)pmd->time + 1), ");
//			printf("framnr: %d, pmd time + 1: %d\n", framenr, (int)pmd->time + 1);
//			return;
//		}

		/* don't simulate if viewing start frame, but scene frame is not real start frame */
		if (framenr != CFRA) {
			printf("%s: %s\n", __func__, "(framenr != CFRA)");
			return;
		}

#ifdef DEBUG_TIME
		double start = PIL_check_seconds_timer();
#endif

		/* if on second frame, write cache for first frame */
		if ((int)pmd->time == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact == 0)) {
			BKE_ptcache_write(&pid, startframe);
		}

		// set new time
		pmd->time = CFRA;

		/* do simulation */

		// simulate the actual smoke (c++ code in intern/smoke)
		// DG: interesting commenting this line + deactivating loading of noise files
		if (framenr != startframe) {
//			if (pds->flags & MOD_SMOKE_DISSOLVE) {
//				smoke_dissolve(pds->fluid, pds->diss_speed, pds->flags & MOD_SMOKE_DISSOLVE_LOG);
//			}

			printf("%s\n", __func__);
			step(scene, ob, pmd, dm);

			/* XXX - do that elsewhere! */
			if (pmd->domain->buffer) {
				printf("Freeing domain buffer!\n");
				MEM_freeN(pmd->domain->buffer);
				pmd->domain->buffer = NULL;
			}
		}

		BKE_ptcache_validate(cache, framenr);
		if (framenr != startframe) {
			BKE_ptcache_write(&pid, framenr);
		}

#ifdef DEBUG_TIME
		double end = PIL_check_seconds_timer();
		printf("Frame: %d, Time: %f\n\n", (int)smd->time, (float)(end - start));
#endif
	}
}

DerivedMesh *BKE_poseidon_modifier_do(PoseidonModifierData *smd, Scene *scene,
                                      Object *ob, DerivedMesh *dm)
{
	printf("%s\n", __func__);
	poseidon_modifier_process(smd, scene, ob, dm);
	return CDDM_copy(dm);
}
