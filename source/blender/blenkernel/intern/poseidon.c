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
#include "BKE_DerivedMesh.h"
#include "BKE_pointcache.h"
#include "BKE_poseidon.h"

#define DT_DEFAULT 0.1f
//#define DEBUG_TIME

#ifdef DEBUG_TIME
#	include "PIL_time.h"
#endif

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

		pmd->domain->cache = BKE_ptcache_add(&(pmd->domain->ptcaches));
		pmd->domain->cache->flag |= PTCACHE_DISK_CACHE;
		pmd->domain->cache->step = 1;

		pmd->domain->fields = MEM_callocN(sizeof(SmokeFields), "SmokeFields");
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

	MEM_freeN(domain->fields);
	domain->fields = NULL;

	MEM_freeN(domain);
	domain = NULL;
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
	flow = NULL;
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
	coll = NULL;
}

void BKE_poseidon_modifier_free(PoseidonModifierData *pmd)
{
	if (!pmd) {
		return;
	}

	poseidon_domain_free(pmd->domain);
	poseidon_flow_free(pmd->flow);
	poseidon_collision_free(pmd->coll);
}

static int poseidon_modifier_init(PoseidonModifierData *pmd, Object *ob, Scene *scene, DerivedMesh *dm)
{
	UNUSED_VARS(ob, dm);
	if ((pmd->type & MOD_SMOKE_TYPE_DOMAIN) && pmd->domain /*&& !smd->domain->fluid*/) {
		pmd->time = CFRA;
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

static void step(Scene *scene, Object *ob, PoseidonModifierData *pmd, DerivedMesh *domain_dm, float fps)
{
	PoseidonDomainSettings *pds = pmd->domain;

	float gravity[3] = { 0.0f, 0.0f, -1.0f };

	/* update object state */
//	invert_m4_m4(pds->imat, ob->obmat);
//	copy_m4_m4(pds->obmat, ob->obmat);
//	smoke_set_domain_from_derivedmesh(sds, ob, domain_dm, (sds->flags & MOD_SMOKE_ADAPTIVE_DOMAIN) != 0);

	/* use global gravity if enabled */
	if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
		copy_v3_v3(gravity, scene->physics_settings.gravity);
		/* map default value to 1.0 */
		mul_v3_fl(gravity, 1.0f / 9.810f);
	}

	/* convert gravity to domain space */
	const float gravity_mag = len_v3(gravity);
//	mul_mat3_m4_v3(pds->imat, gravity);
	normalize_v3(gravity);
	mul_v3_fl(gravity, gravity_mag);

	/* adapt timestep for different framerates, dt = 0.1 is at 25fps */
	const float dt = DT_DEFAULT * (25.0f / fps);

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
//		update_flowsfluids(scene, ob, pds, dtSubdiv);
//		update_obstacles(scene, ob, pds, dtSubdiv, substep, totalSubsteps);

//		if (pds->total_cells > 1) {
//			update_effectors(scene, ob, pds, dtSubdiv); // DG TODO? problem --> uses forces instead of velocity, need to check how they need to be changed with variable dt
//			smoke_step(pds->fluid, gravity, dtSubdiv);
//		}
	}
}

static void poseidon_modifier_process(PoseidonModifierData *pmd, Scene *scene,
                                      Object *ob, DerivedMesh *dm)
{
	if ((pmd->type & MOD_SMOKE_TYPE_FLOW)) {
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
		PoseidonDomainSettings *pds = pmd->domain;
		int startframe, endframe;
		float timescale;

		int framenr = CFRA;

		PointCache *cache = pds->cache;
		PTCacheID pid;
		BKE_ptcache_id_from_poseidon(&pid, ob, pmd);
		BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

		if (/*!pds->fluid ||*/ framenr == startframe) {
			BKE_poseidon_modifier_reset(pmd);

			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_validate(cache, framenr);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}

//		if (!pds->fluid && (framenr != startframe) && (pds->flags & MOD_SMOKE_FILE_LOAD) == 0 && (cache->flag & PTCACHE_BAKED) == 0)
//			return;

//		pds->flags &= ~MOD_SMOKE_FILE_LOAD;
		CLAMP(framenr, startframe, endframe);

		/* If already viewing a pre/after frame, no need to reload */
		if ((pmd->time == framenr) && (framenr != CFRA)) {
			return;
		}

		if (!poseidon_modifier_init(pmd, ob, scene, dm)) {
			printf("Warning: bad Poseidon initialization!\n");
			return;
		}

		/* try to read from cache */
		if (BKE_ptcache_read(&pid, (float)framenr) == PTCACHE_READ_EXACT) {
			BKE_ptcache_validate(cache, framenr);
			pmd->time = framenr;
			return;
		}

		/* only calculate something when we advanced a single frame */
		if (framenr != (int)pmd->time + 1) {
			return;
		}

		/* don't simulate if viewing start frame, but scene frame is not real start frame */
		if (framenr != CFRA) {
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

			step(scene, ob, pmd, dm, scene->r.frs_sec / scene->r.frs_sec_base);
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
	poseidon_modifier_process(smd, scene, ob, dm);
	return CDDM_copy(dm);
}
