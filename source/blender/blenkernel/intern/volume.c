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

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_volume.h"

#include "GPU_draw.h"

#include "openvdb_capi.h"

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

	VolumeData *data;
	while ((data = BLI_pophead(&volume->fields)) != NULL) {
		OpenVDBPrimitive_free(data->prim);

		if (data->buffer) {
			MEM_freeN(data->buffer);
		}

		if (data->draw_nodes) {
			for (int i = 0; i < data->num_draw_nodes; i++) {
				if (data->draw_nodes[i])
					GPU_volume_node_free(data->draw_nodes[i]);
			}

			MEM_freeN(data->draw_nodes);
		}

		MEM_freeN(data);
	}
}

VolumeData *BKE_volume_field_current(const Volume *volume)
{
	VolumeData *data = volume->fields.first;

	for (; data; data = data->next) {
		if (data->flags & VOLUME_DATA_CURRENT) {
			return data;
		}
	}

	return data;
}

BoundBox *BKE_volume_boundbox_get(Object *ob)
{
	const Volume *volume = BKE_volume_from_object(ob);
	VolumeData *data = BKE_volume_field_current(volume);

	if (ob->bb == NULL) {
		ob->bb = BKE_boundbox_alloc_unit();
	}

	BKE_boundbox_init_from_minmax(ob->bb, data->bbmin, data->bbmax);

	return ob->bb;
}

void BKE_volume_update(Scene *scene, Object *ob)
{
	Volume *volume = BKE_volume_from_object(ob);

	if (volume->filename[0] == '\0' && !volume->has_file_sequence) {
		return;
	}

	int frame, num_digits;
	BLI_path_frame_get(volume->filename, &frame, &num_digits);

	if (frame == CFRA) {
		return;
	}

	char ext[FILE_MAX];
	char new_filename[FILE_MAX];
	BLI_strncpy(new_filename, volume->filename, sizeof(volume->filename));
	BLI_path_frame_strip(new_filename, true, ext);
	BLI_path_frame(new_filename, CFRA, num_digits);
	BLI_ensure_extension(new_filename, sizeof(new_filename), ".vdb");

	if (!BLI_exists(new_filename)) {
		return;
	}

	BKE_volume_free(volume);
	BKE_volume_load_from_file(volume, new_filename);
}

static void openvdb_get_grid_info(void *userdata, const char *name,
                                  const char *value_type, bool is_color,
                                  struct OpenVDBPrimitive *prim)
{
	Volume *volume = userdata;
	VolumeData *data = MEM_mallocN(sizeof(VolumeData), "VolumeData");

	BLI_strncpy(data->name, name, sizeof(data->name));

	if (STREQ(value_type, "float")) {
		data->type = VOLUME_TYPE_FLOAT;
	}
	else if (STREQ(value_type, "vec3s")) {
		if (is_color)
			data->type = VOLUME_TYPE_COLOR;
		else
			data->type = VOLUME_TYPE_VEC3;
	}
	else {
		data->type = VOLUME_TYPE_UNKNOWN;
	}

	data->prim = prim;
	data->buffer = NULL;
	data->flags = 0;
	data->display_mode = 0;
	data->draw_nodes = NULL;
	data->num_draw_nodes = 0;
	zero_v3(data->bbmin);
	zero_v3(data->bbmax);

	BLI_addtail(&volume->fields, data);
}

void BKE_volume_load_from_file(Volume *volume, const char *filename)
{
	OpenVDB_get_grid_info(filename, openvdb_get_grid_info, volume);
	BLI_strncpy(volume->filename, filename, sizeof(volume->filename));

	/* Set the first volume field as the current one */
	VolumeData *data = volume->fields.first;
	data->flags |= VOLUME_DATA_CURRENT;
}
