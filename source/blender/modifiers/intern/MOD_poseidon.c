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
 * The Original Code is Copyright (C) 2016 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "DNA_group_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_poseidon_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_poseidon.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md)
{
	PoseidonModifierData *pmd = (PoseidonModifierData *) md;

	pmd->coll = NULL;
	pmd->domain = NULL;
	pmd->flow = NULL;
	pmd->time = -1;
	pmd->type = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	modifier_copyData_generic(md, target);
}

static void freeData(ModifierData *md)
{
	PoseidonModifierData *pmd = (PoseidonModifierData *) md;

	BKE_poseidon_modifier_free(pmd);
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	PoseidonModifierData *pmd = (PoseidonModifierData *) md;

	if (flag & MOD_APPLY_ORCO)
		return dm;

	return BKE_poseidon_modifier_do(pmd, md->scene, ob, dm);
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void update_depsgraph_flow_coll_object(DagForest *forest,
                                              DagNode *obNode,
                                              Object *object2)
{
	PoseidonModifierData *smd;
	if ((object2->id.tag & LIB_TAG_DOIT) == 0) {
		return;
	}
	object2->id.tag &= ~LIB_TAG_DOIT;
	smd = (PoseidonModifierData *)modifiers_findByType(object2, eModifierType_Poseidon);
	if (smd && (((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) ||
	            ((smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll)))
	{
		DagNode *curNode = dag_get_node(forest, object2);
		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Poseidon Flow/Coll");
	}
	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_flow_coll_object(forest, obNode, go->ob);
		}
	}
}

static void update_depsgraph_field_source_object(DagForest *forest,
                                                 DagNode *obNode,
                                                 Object *object,
                                                 Object *object2)
{
	if ((object2->id.tag & LIB_TAG_DOIT) == 0) {
		return;
	}
	object2->id.tag &= ~LIB_TAG_DOIT;

	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_field_source_object(forest, obNode, object, go->ob);
		}
	}
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *bmain,
                           struct Scene *scene, struct Object *ob,
                           DagNode *obNode)
{
	PoseidonModifierData *smd = (PoseidonModifierData *) md;
	Base *base;

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		if (smd->domain->fluid_group || smd->domain->coll_group) {
			GroupObject *go = NULL;

			if (smd->domain->fluid_group)
				for (go = smd->domain->fluid_group->gobject.first; go; go = go->next) {
					if (go->ob) {
						PoseidonModifierData *smd2 = (PoseidonModifierData *)modifiers_findByType(go->ob, eModifierType_Poseidon);

						/* check for initialized smoke object */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
							DagNode *curNode = dag_get_node(forest, go->ob);
							dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Poseidon Flow");
						}
					}
				}

			if (smd->domain->coll_group)
				for (go = smd->domain->coll_group->gobject.first; go; go = go->next) {
					if (go->ob) {
						PoseidonModifierData *smd2 = (PoseidonModifierData *)modifiers_findByType(go->ob, eModifierType_Poseidon);

						/* check for initialized smoke object */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll) {
							DagNode *curNode = dag_get_node(forest, go->ob);
							dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Poseidon Coll");
						}
					}
				}
		}
		else {
			BKE_main_id_tag_listbase(&bmain->object, LIB_TAG_DOIT, true);
			base = scene->base.first;
			for (; base; base = base->next) {
				update_depsgraph_flow_coll_object(forest, obNode, base->object);
			}
		}
		/* add relation to all "smoke flow" force fields */
		base = scene->base.first;
		BKE_main_id_tag_listbase(&bmain->object, LIB_TAG_DOIT, true);
		for (; base; base = base->next) {
			update_depsgraph_field_source_object(forest, obNode, ob, base->object);
		}
	}
}

static void update_depsgraph_flow_coll_object_new(struct DepsNodeHandle *node,
                                                  Object *object2)
{
	PoseidonModifierData *smd;
	if ((object2->id.tag & LIB_TAG_DOIT) == 0) {
		return;
	}
	object2->id.tag &= ~LIB_TAG_DOIT;
	smd = (PoseidonModifierData *)modifiers_findByType(object2, eModifierType_Poseidon);
	if (smd && (((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) ||
	            ((smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll)))
	{
		DEG_add_object_relation(node, object2, DEG_OB_COMP_TRANSFORM, "Poseidon Flow/Coll");
		DEG_add_object_relation(node, object2, DEG_OB_COMP_GEOMETRY, "Poseidon Flow/Coll");
	}
	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_flow_coll_object_new(node, go->ob);
		}
	}
}

static void update_depsgraph_field_source_object_new(struct DepsNodeHandle *node,
                                                     Object *object,
                                                     Object *object2)
{
	if ((object2->id.tag & LIB_TAG_DOIT) == 0) {
		return;
	}
	object2->id.tag &= ~LIB_TAG_DOIT;

	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_field_source_object_new(node, object, go->ob);
		}
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *bmain,
                            struct Scene *scene,
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	PoseidonModifierData *smd = (PoseidonModifierData *)md;
	Base *base;
	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		if (smd->domain->fluid_group || smd->domain->coll_group) {
			GroupObject *go = NULL;
			if (smd->domain->fluid_group != NULL) {
				for (go = smd->domain->fluid_group->gobject.first; go; go = go->next) {
					if (go->ob != NULL) {
						PoseidonModifierData *smd2 = (PoseidonModifierData *)modifiers_findByType(go->ob, eModifierType_Poseidon);
						/* Check for initialized smoke object. */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
							DEG_add_object_relation(node, go->ob, DEG_OB_COMP_TRANSFORM, "Poseidon Flow");
						}
					}
				}
			}
			if (smd->domain->coll_group != NULL) {
				for (go = smd->domain->coll_group->gobject.first; go; go = go->next) {
					if (go->ob != NULL) {
						PoseidonModifierData *smd2 = (PoseidonModifierData *)modifiers_findByType(go->ob, eModifierType_Poseidon);
						/* Check for initialized smoke object. */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll) {
							DEG_add_object_relation(node, go->ob, DEG_OB_COMP_TRANSFORM, "Poseidon Coll");
						}
					}
				}
			}
		}
		else {
			BKE_main_id_tag_listbase(&bmain->object, LIB_TAG_DOIT, true);
			base = scene->base.first;
			for (; base; base = base->next) {
				update_depsgraph_flow_coll_object_new(node, base->object);
			}
		}
		/* add relation to all "smoke flow" force fields */
		base = scene->base.first;
		BKE_main_id_tag_listbase(&bmain->object, LIB_TAG_DOIT, true);
		for (; base; base = base->next) {
			update_depsgraph_field_source_object_new(node, ob, base->object);
		}
	}
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	PoseidonModifierData *pmd = (PoseidonModifierData *) md;

	if (pmd->type == MOD_SMOKE_TYPE_DOMAIN && pmd->domain) {
		walk(userData, ob, (ID **)&pmd->domain->coll_group, IDWALK_NOP);
		walk(userData, ob, (ID **)&pmd->domain->fluid_group, IDWALK_NOP);
	}
}

ModifierTypeInfo modifierType_Poseidon = {
	/* name */              "Poseidon",
	/* structName */        "PoseidonModifierData",
	/* structSize */        sizeof(PoseidonModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_UsesPointCache |
	                        eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL
};
