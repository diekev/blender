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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_volume_types.h
 *  \ingroup dna
 *  \since January 2016
 *  \author Kevin Dietrich
 */

#ifndef __DNA_VOLUME_TYPES_H__
#define __DNA_VOLUME_TYPES_H__

#include "DNA_ID.h"

typedef enum eVolumeDataType {
	VOLUME_TYPE_UNKNOWN  = 0,
	VOLUME_TYPE_FLOAT    = 1,
	VOLUME_TYPE_VEC3     = 2,
	VOLUME_TYPE_COLOR    = 3,
	VOLUME_TYPE_INT      = 4,
} eVolumeDataType;

enum {
	VOLUME_DATA_CURRENT  = (1 << 0),
	VOLUME_DRAW_TOPOLOGY = (1 << 1),
};

typedef struct VolumeData {
	struct VolumeData *next, *prev;

	struct OpenVDBPrimitive *prim;
	struct VolumeDrawNode **draw_nodes;

	/* dense buffer */
	float *buffer;
	int res[3];
	short pad2;
	short num_draw_nodes;

	float bbmin[3], bbmax[3];

	char name[64];  /* MAX_NAME */
	short type;
	short flags;
	short pad[2];
} VolumeData;

typedef struct Volume {
	ID id;

	ListBase fields;
} Volume;

#endif /* __DNA_VOLUME_TYPES_H__ */
