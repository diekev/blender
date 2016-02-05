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

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_modifier.h"
#include "BKE_poseidon.h"

static void initData(ModifierData *md)
{
	PoseidonModifierData *pmd = (PoseidonModifierData *) md;

	pmd->coll = NULL;
	pmd->domain = NULL;
	pmd->flow = NULL;
	pmd->time = 0;
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
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL
};
