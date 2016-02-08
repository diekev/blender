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

/** \file blender/editors/io/io_volume.c
 *  \ingroup io
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_volume_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_volume.h"

#include "RNA_access.h"

#include "ED_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "io_volume.h"

#include "openvdb_capi.h"

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

static int wm_volume_import_exec(bContext *C, wmOperator *op)
{
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}

	char filename[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filename);

	BKE_scene_base_deselect_all(CTX_data_scene(C));

	float loc[3], rot[3];
	bool enter_editmode;
	unsigned int layer;

	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	Object *ob = ED_object_add_type(C, OB_VOLUME, NULL, loc, rot, true, layer);

	Volume *volume = BKE_volume_from_object(ob);

	if (!volume) {
		BKE_report(op->reports, RPT_ERROR, "Cannot create volume!");
		return OPERATOR_CANCELLED;
	}

	OpenVDB_get_grid_info(filename, openvdb_get_grid_info, volume);

	/* Set the first volume field as the current one */
	VolumeData *data = volume->fields.first;
	data->flags |= VOLUME_DATA_CURRENT;

	return OPERATOR_FINISHED;
}

void WM_OT_volume_import(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Import OpenVDB Volume";
	ot->description = "Import an OpenVDB volume object to the scene";
	ot->idname = "WM_OT_volume_import";

	/* api callbacks */
	ot->invoke = WM_operator_filesel;
	ot->exec = wm_volume_import_exec;
	ot->poll = WM_operator_winactive;

	/* properties */
	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);

	WM_operator_properties_filesel(ot,
	                               FILE_TYPE_FOLDER | FILE_TYPE_OPENVDB,
	                               FILE_BLENDER,
	                               FILE_OPENFILE,
	                               WM_FILESEL_FILEPATH,
	                               FILE_DEFAULTDISPLAY,
	                               FILE_SORT_ALPHA);
}
