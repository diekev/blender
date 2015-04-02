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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"

#include "DNA_object_types.h"

#include "MOD_openvdb_util.h"

#include "openvdb_capi.h"

typedef struct DMArrays {
	MVert *mvert;
	MLoop *mloop;
	MPoly *mpoly;
	bool mvert_allocated;
	bool mloop_allocated;
	bool mpoly_allocated;
} DMArrays;

static void dm_arrays_get(DerivedMesh *dm, DMArrays *arrays)
{
	arrays->mvert = DM_get_vert_array(dm, &arrays->mvert_allocated);
	arrays->mloop = DM_get_loop_array(dm, &arrays->mloop_allocated);
	arrays->mpoly = DM_get_poly_array(dm, &arrays->mpoly_allocated);
}

static void dm_arrays_free(DMArrays *arrays)
{
	if (arrays->mvert_allocated) {
		MEM_freeN(arrays->mvert);
	}
	if (arrays->mloop_allocated) {
		MEM_freeN(arrays->mloop);
	}
	if (arrays->mpoly_allocated) {
		MEM_freeN(arrays->mpoly);
	}
}

typedef struct ImportMeshData {
	DerivedMesh *dm;
	float obmat[4][4];
	MVert *mvert;
	MPoly *mpoly;
	MLoop *mloop;
} ImportMeshData;

/* Get number of verts. */
static int importer_getNumVerts(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumVerts(dm);
}

/* Get number of loops. */
static int importer_getNumLoops(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumLoops(dm);
}

/* Get number of polys. */
static int importer_getNumPolys(ImportMeshData *import_data)
{
	DerivedMesh *dm = import_data->dm;
	return dm->getNumPolys(dm);
}

/* Get 3D coordinate of vertex with given index. */
static void importer_getVertCoord(ImportMeshData *import_data, int vert_index, float coord[3])
{
	MVert *mvert = import_data->mvert;

	BLI_assert(vert_index >= 0 && vert_index < import_data->dm->getNumVerts(import_data->dm));

	mul_v3_m4v3(coord, import_data->obmat, mvert[vert_index].co);
}

/* Get number of adjacent vertices to the poly specified by its index. */
static int importer_GetPolyNumVerts(ImportMeshData *import_data, int poly_index)
{
	MPoly *mpoly = import_data->mpoly;

	BLI_assert(poly_index >= 0 && poly_index < import_data->dm->getNumPolys(import_data->dm));

	return mpoly[poly_index].totloop;
}

/* Get list of adjacent vertices to the poly specified by its index. */
static void importer_GetPolyVerts(ImportMeshData *import_data, int poly_index, int *verts)
{
	MPoly *mpoly = &import_data->mpoly[poly_index];
	MLoop *mloop = import_data->mloop + mpoly->loopstart;
	int i;
	BLI_assert(poly_index >= 0 && poly_index < import_data->dm->getNumPolys(import_data->dm));
	for (i = 0; i < mpoly->totloop; i++, mloop++) {
		verts[i] = mloop->v;
	}
}

static VDBMeshImporter MeshImporter = {
	importer_getNumVerts,
	importer_getNumLoops,
	importer_getNumPolys,
	importer_getVertCoord,
	importer_GetPolyNumVerts,
	importer_GetPolyVerts,
};

/* **** Exporter from VDB to derived mesh ****  */

typedef struct ExportMeshData {
	DerivedMesh *dm;
	float obimat[4][4];
	MVert *mvert;
	MLoop *mloop;
	MPoly *mpoly;
	int *vert_origindex;
	int *poly_origindex;
	int *loop_origindex;

	/* Objects and derived meshes of left and cutter operands.
	 * Used for custom data merge and interpolation.
	 */
	Object *ob_left;
	Object *ob_cutter;
	DerivedMesh *dm_left;
	DerivedMesh *dm_cutter;
	MVert *mvert_left;
	MLoop *mloop_left;
	MPoly *mpoly_left;
	MVert *mvert_cutter;
	MLoop *mloop_cutter;
	MPoly *mpoly_cutter;

	float left_to_cutter_mat[4][4];
} ExportMeshData;

/* Create new external mesh */
static void exporter_InitGeomArrays(ExportMeshData *export_data,
									int num_verts, int num_loops, int num_polys)
{
	DerivedMesh *dm = CDDM_new(num_verts, 0, 0, num_loops, num_polys);
	DerivedMesh *dm_left = export_data->dm_left;
//				*dm_cutter = export_data->dm_cutter;

	/* Mask for custom data layers to be merged from operands. */
	CustomDataMask merge_mask = CD_MASK_DERIVEDMESH & ~CD_MASK_ORIGINDEX;

	export_data->dm = dm;
	export_data->mvert = dm->getVertArray(dm);
	export_data->mloop = dm->getLoopArray(dm);
	export_data->mpoly = dm->getPolyArray(dm);

	/* Merge custom data layers from operands.
	 *
	 * Will only create custom data layers for all the layers which appears in
	 * the operand. Data for those layers will not be allocated or initialized.
	 */

	CustomData_merge(&dm_left->loopData, &dm->loopData, merge_mask, CD_DEFAULT, num_loops);
	CustomData_merge(&dm_left->polyData, &dm->polyData, merge_mask, CD_DEFAULT, num_polys);

//	if (dm_cutter != NULL) {
//		CustomData_merge(&dm_cutter->loopData, &dm->loopData, merge_mask, CD_DEFAULT, num_loops);
//		CustomData_merge(&dm_cutter->polyData, &dm->polyData, merge_mask, CD_DEFAULT, num_polys);
//	}

	export_data->vert_origindex = dm->getVertDataArray(dm, CD_ORIGINDEX);
	export_data->poly_origindex = dm->getPolyDataArray(dm, CD_ORIGINDEX);
	export_data->loop_origindex = dm->getLoopDataArray(dm, CD_ORIGINDEX);
}

/* Set coordinate of vertex with given index. */
static void exporter_SetVert(ExportMeshData *export_data,
							 int vert_index, float coord[3])
{
	DerivedMesh *dm = export_data->dm;
	MVert *mvert = export_data->mvert;

	BLI_assert(vert_index >= 0 && vert_index <= dm->getNumVerts(dm));

	export_data->vert_origindex[vert_index] = ORIGINDEX_NONE;

	mul_v3_m4v3(mvert[vert_index].co, export_data->obimat, coord);
}


/* Set list of adjucent loops to the poly specified by it's index. */
static void exporter_SetPoly(ExportMeshData *export_data,
							 int poly_index, int start_loop, int num_loops)
{
	DerivedMesh *dm = export_data->dm;
	DerivedMesh *dm_orig;
	MPoly *mpoly = &export_data->mpoly[poly_index];

	/* Poly is always to be either from left or cutter operand. */
	dm_orig = export_data->dm_left;

	BLI_assert(poly_index >= 0 && poly_index < dm->getNumPolys(dm));
	BLI_assert(start_loop >= 0 && start_loop <= dm->getNumLoops(dm) - num_loops);
	BLI_assert(num_loops >= 3);
	BLI_assert(dm_orig != NULL);
	//BLI_assert(orig_poly_index >= 0 && orig_poly_index < dm_orig->getNumPolys(dm_orig));

	/* Copy all poly layers, including mpoly. */
//	mpoly = export_data->mpoly_left;
//	CustomData_copy_data(&dm_orig->polyData, &dm->polyData, ORIGINDEX_NONE, poly_index, 1);

	/* Set original index of the poly. */
	if (export_data->poly_origindex) {
		export_data->poly_origindex[poly_index] = ORIGINDEX_NONE;
	}

	/* Set poly data itself. */
	mpoly->loopstart = start_loop;
	mpoly->totloop = num_loops;

	/* XXX this is supposed to be set by the user in the UI */
	mpoly->flag |= ME_SMOOTH;
}

/* Set list vertex and edge which are adjucent to loop with given index. */
static void exporter_SetLoop(ExportMeshData *export_data, int loop_index, int vertex)
{
	DerivedMesh *dm = export_data->dm;
//	DerivedMesh *dm_orig;
	MLoop *mloop = &export_data->mloop[loop_index];

	BLI_assert(loop_index >= 0 && loop_index < dm->getNumLoops(dm));
	BLI_assert(vertex >= 0 && vertex < dm->getNumVerts(dm));

//	dm_orig = export_data->dm_left;
//	if (dm_orig) {
//		BLI_assert(orig_loop_index >= 0 && orig_loop_index < dm_orig->getNumLoops(dm_orig));

//		/* Copy all loop layers, including mloop. */
//		mloop = export_data->mloop_left;
//		CustomData_copy_data(&dm_orig->loopData, &dm->loopData, ORIGINDEX_NONE, loop_index, 1);
//	}

	/* Set original index of the loop. */
	if (export_data->loop_origindex) {
		export_data->loop_origindex[loop_index] = ORIGINDEX_NONE;
	}

	mloop->v = vertex;
}

static VDBMeshExporter MeshExporter = {
	exporter_InitGeomArrays,
	exporter_SetVert,
	exporter_SetPoly,
	exporter_SetLoop,
};

static void prepare_import_data(Object *object,
								DerivedMesh *dm,
								const DMArrays *dm_arrays,
								ImportMeshData *import_data)
{
	import_data->dm = dm;
	copy_m4_m4(import_data->obmat, object->obmat);
	import_data->mvert = dm_arrays->mvert;
	import_data->mloop = dm_arrays->mloop;
	import_data->mpoly = dm_arrays->mpoly;
}

static struct VDBMeshDescr *vdb_mesh_from_dm(Object *object,
											 DerivedMesh *dm,
											 const DMArrays *dm_arrays)
{
	ImportMeshData import_data;
	prepare_import_data(object, dm, dm_arrays, &import_data);
	return VDB_addMesh(&import_data, &MeshImporter);
}

static void prepare_export_data(Object *object, DerivedMesh *dm, const DMArrays *dm_arrays,								
								Object *object_cutter, DerivedMesh *dm_cutter, const DMArrays *dm_cutter_arrays,
								ExportMeshData *export_data)
{
	float object_cutter_imat[4][4];

	invert_m4_m4(export_data->obimat, object->obmat);

	export_data->ob_left = object;

	export_data->dm_left = dm;

	export_data->mvert_left = dm_arrays->mvert;
	export_data->mloop_left = dm_arrays->mloop;
	export_data->mpoly_left = dm_arrays->mpoly;

	if (dm_cutter != NULL) {
		export_data->dm_cutter = dm_cutter;
		export_data->mvert_cutter = dm_cutter_arrays->mvert;
		export_data->mloop_cutter = dm_cutter_arrays->mloop;
		export_data->mpoly_cutter = dm_cutter_arrays->mpoly;

		/* Matrix to convert coord from left object's loca; space to
		 * cutter object's local space.
		 */
		invert_m4_m4(object_cutter_imat, object_cutter->obmat);
		mul_m4_m4m4(export_data->left_to_cutter_mat, object->obmat,
					object_cutter_imat);

	}
}

DerivedMesh *NewParticleDerivedMesh(DerivedMesh *dm, struct Object *ob,
									DerivedMesh *cutter_dm, struct Object *cutter_ob,
									struct ParticleMesherModifierData *pmmd,
									struct Scene *scene)
{

	struct VDBMeshDescr *mask_mesh = NULL, *output = NULL;
	DerivedMesh *output_dm = NULL;
	bool result;
	DMArrays dm_arrays, cutter_dm_arrays;

	if (dm == NULL) {
		return NULL;
	}

	dm_arrays_get(dm, &dm_arrays);

	if (cutter_dm != NULL) {
		dm_arrays_get(cutter_dm, &cutter_dm_arrays);

		mask_mesh = vdb_mesh_from_dm(cutter_ob, cutter_dm, &cutter_dm_arrays);
	}

	result = OpenVDB_performParticleSurfacing(scene, ob, pmmd, mask_mesh, &output);
	VDB_deleteMesh(mask_mesh);

	if (result) {
		ExportMeshData export_data;

		prepare_export_data(ob, dm, &dm_arrays,
							cutter_ob, cutter_dm, &cutter_dm_arrays,
							&export_data);

		VDB_exportMesh(output, &MeshExporter, &export_data);
		output_dm = export_data.dm;

		CDDM_calc_edges(output_dm);

		output_dm->cd_flag |= dm->cd_flag;
		output_dm->dirty |= DM_DIRTY_NORMALS;
		VDB_deleteMesh(output);
	}

	dm_arrays_free(&dm_arrays);

	if (cutter_dm != NULL) {
		dm_arrays_free(&cutter_dm_arrays);
	}

	return output_dm;
}
