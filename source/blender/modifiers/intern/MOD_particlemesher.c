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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_particlemesher.c
 *  \ingroup modifiers
 */

/* Particle mesher modifier: generates a mesh from a particle system */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"

#include "DNA_scene_types.h"

#include "MOD_util.h"
#include "MOD_openvdb_util.h"

#include "depsgraph_private.h"

#ifdef WITH_MOD_PARTMESHER
#include "openvdb_capi.h"
#endif

static void initData(ModifierData *md)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;

	pmmd->psys = NULL;
	pmmd->part_list = NULL;
	pmmd->level_set = NULL;
	pmmd->mesher_mask = NULL;
	pmmd->mesher_mask_ob = NULL;
	pmmd->voxel_size = 0.2f;
	pmmd->min_part_radius = 1.5f;
	pmmd->half_width = 0.75f;
	pmmd->generate_trails = false;
	pmmd->part_scale_factor = 1.0f;
	pmmd->part_vel_factor = 1.0f;
	pmmd->trail_size = 1.0f;
	pmmd->adaptivity = 0.0f;
	pmmd->isovalue = 0.0f;
	pmmd->generate_mask = false;
	pmmd->mask_width = 0.0f;
	pmmd->mask_offset = 0.0f;
	pmmd->invert_mask = false;
	pmmd->ext_band = 3.0f;
	pmmd->int_band = 3.0f;
	pmmd->draw_vdb_tree = (VDB_DRAW_LEAVES | VDB_DRAW_LEVEL_1 | VDB_DRAW_LEVEL_2 | VDB_DRAW_ROOT);
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;
	ParticleMesherModifierData *tpmmd = (ParticleMesherModifierData *) target;
#endif

	modifier_copyData_generic(md, target);
}

static bool isDisabled(ModifierData *md, int useRenderParams)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;

	return !pmmd->psys;

	UNUSED_VARS(useRenderParams);
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk,
                              void *userData)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;

	if (pmmd->mesher_mask_ob) {
		walk(userData, ob, &pmmd->mesher_mask_ob, IDWALK_NOP);
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *bmain,
                            struct Scene *scene,
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;

	if (pmmd->mesher_mask_ob != NULL) {
		DEG_add_object_relation(node, pmmd->mesher_mask_ob, DEG_OB_COMP_TRANSFORM,
		                        "Particle Mesher Modifier");
	}

	UNUSED_VARS(bmain, scene, ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *bmain,
                           struct Scene *scene,
                           Object *ob,
                           DagNode *obNode)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;

	if (pmmd->mesher_mask_ob) {
		DagNode *curNode = dag_get_node(forest, pmmd->mesher_mask_ob);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
		                 "Particle Mesher Modifier");
	}

	UNUSED_VARS(bmain, scene, ob);
}

#ifdef WITH_MOD_PARTMESHER
static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;
	DerivedMesh *result, *mask_dm = NULL;

	if (pmmd->mesher_mask_ob) {
		mask_dm = get_dm_for_modifier(pmmd->mesher_mask_ob, flag);
	}

	result = NewParticleDerivedMesh(dm, ob, mask_dm, pmmd->mesher_mask_ob, pmmd, md->scene);

	if (result) {
		return result;
	}
	else {
		modifier_setError(md, "Cannot execute modifier");
	}

	return dm;
}
#else /* WITH_MOD_PARTMESHER */
static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm,
								  ModifierApplyFlag flag)
{
	return dm;
	UNUSED_VARS(md, ob, flag);
}
#endif /* WITH_MOD_PARTMESHER */

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = CD_MASK_MTFACE | CD_MASK_MEDGE;

	dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;

	UNUSED_VARS(ob, md);
}

static void freeData(ModifierData *md)
{
	ParticleMesherModifierData *pmmd = (ParticleMesherModifierData *) md;
	LevelSetFilter *filter = pmmd->filters.first;

	for (; filter; filter = filter->next) {
		MEM_freeN(filter);
	}

#ifdef WITH_MOD_PARTMESHER
	if (pmmd->part_list) {
		OpenVDB_part_list_free(pmmd->part_list);
	}

	if (pmmd->level_set) {
		OpenVDBPrimitive_free(pmmd->level_set);
	}

	if (pmmd->mesher_mask) {
		OpenVDBPrimitive_free(pmmd->mesher_mask);
	}
#endif
}

ModifierTypeInfo modifierType_ParticleMesher = {
    /* name */              "Particle Mesher",
    /* structName */        "ParticleMesherModifierData",
    /* structSize */        sizeof(ParticleMesherModifierData),
    /* type */              eModifierTypeType_Constructive,
    /* flags */             eModifierTypeFlag_AcceptsMesh |
							eModifierTypeFlag_SupportsEditmode |
							eModifierTypeFlag_EnableInEditmode,

    /* copyData */          copyData,
    /* deformVerts */       NULL,
    /* deformMatrices */    NULL,
    /* deformVertsEM */     NULL,
    /* deformMatricesEM */  NULL,
    /* applyModifier */     applyModifier,
    /* applyModifierEM */   NULL,
    /* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
    /* isDisabled */        isDisabled,
    /* updateDepgraph */    updateDepgraph,
    /* updateDepsgraph */   updateDepsgraph,
    /* dependsOnTime */     NULL,
    /* dependsOnNormals */  NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */     NULL,
    /* foreachTexLink */    NULL,
};
