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

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_volume.h"

#include "GPU_draw.h"

#include "openvdb_capi.h"
#include "openvdb_mesh_capi.h"

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

void BKE_volume_make_local(Volume *volume)
{
	if (volume->id.lib == NULL) {
		return;
	}

	Main *bmain = G.main;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if (volume->id.us == 1) {
		id_clear_lib_data(bmain, &volume->id);
		return;
	}

	bool is_local = false, is_lib = false;

	Object *ob;
	for (ob = bmain->object.first; ob && ELEM(false, is_lib, is_local); ob = ob->id.next) {
		if (ob->data == volume) {
			*((ob->id.lib) ? &is_lib : &is_local) = true;
		}
	}

	if (is_local && is_lib == false) {
		id_clear_lib_data(bmain, &volume->id);
	}
	else if (is_local && is_lib) {
		Volume *volume_new = BKE_volume_copy(volume);
		volume_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, volume->id.lib, &volume_new->id);

		ob = bmain->object.first;
		while (ob) {
			if (ob->data == volume) {

				if (ob->id.lib == NULL) {
					ob->data = volume_new;
					id_us_plus(&volume_new->id);
					id_us_min(&volume->id);
				}
			}

			ob = ob->id.next;
		}
	}
}

Volume *BKE_volume_copy(Volume *volume)
{
	Volume *copy = BKE_libblock_copy(&volume->id);

	/* TODO: copy data. */

	if (volume->id.lib) {
		BKE_id_lib_local_paths(G.main, volume->id.lib, &copy->id);
	}

	return copy;
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

/* ***************************** mesh conversion **************************** */

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

void BKE_mesh_to_volume(Scene *scene, Object *ob)
{
	/* make new mesh data from the original copy */
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_MESH);

	Volume *volume = BKE_volume_add(G.main, ob->id.name + 2);
	bool needsFree = false;

	OpenVDBDerivedMeshIterator it;
	openvdb_dm_iter_init(&it, dm);

	struct OpenVDBPrimitive *prim = OpenVDBPrimitive_from_mesh(&it.base, ob->obmat, 0.01f);

	if (volume && prim) {
		VolumeData *data = MEM_callocN(sizeof(VolumeData), "VolumeData");
		data->prim = prim;
		data->flags |= VOLUME_DATA_CURRENT;

		BLI_addtail(&volume->fields, data);

		id_us_min(&((Mesh *)ob->data)->id);

		ob->data = volume;
		ob->type = OB_VOLUME;

		needsFree = true;
	}

	dm->needsFree = needsFree;
	dm->release(dm);

	if (needsFree) {
		ob->derivedFinal = NULL;

		/* curve object could have got bounding box only in special cases */
		if (ob->bb) {
			MEM_freeN(ob->bb);
			ob->bb = NULL;
		}
	}
}
