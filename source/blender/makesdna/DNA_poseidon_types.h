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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_poseidon_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_POSEIDON_TYPES_H__
#define __DNA_POSEIDON_TYPES_H__

#include "DNA_listBase.h"

typedef struct PoseidonFlowSettings {
	struct PoseidonModifierData *pmd;
	struct DerivedMesh *dm;
	float *verts_old;
	int numverts;
	int pad;
} PoseidonFlowSettings;

typedef struct PoseidonCollSettings {
	struct PoseidonModifierData *pmd;
	struct DerivedMesh *dm;
	float *verts_old;
	int numverts;
	short type;  /* static = 0, rigid = 1, dynamic = 2 */
	short pad;
} PoseidonCollSettings;

typedef struct PoseidonDomainSettings {
	struct PoseidonModifierData *pmd;
	struct Group *fluid_group;
	struct Group *coll_group; // collision objects group

	struct PointCache *cache;
	ListBase ptcaches;

	struct PoseidonData *data;

	float obmat[4][4];
	float imat[4][4];

	float voxel_size;
	int pad;
} PoseidonDomainSettings;

#endif /* __DNA_POSEIDON_TYPES_H__ */
