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

/** \file blender/blenkernel/intern/volume.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_volume.h"

Volume *BKE_volume_add(Main *bmain, const char *name)
{
	Volume *volume = BKE_libblock_alloc(bmain, ID_VL, name);
	return volume;
}

Volume *BKE_volume_from_object(Object *ob)
{
	if (ob->type == OB_VOLUME) {
		return (Volume *)ob->data;
	}

	return NULL;
}

void BKE_volume_free(Volume *volume)
{
	if (volume == NULL) {
		return;
	}
}
