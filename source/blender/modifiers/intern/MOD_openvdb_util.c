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
#include "BKE_lattice.h"
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

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
} ExportMeshData;

/* Create new external mesh */
static void exporter_InitGeomArrays(ExportMeshData *export_data,
									int num_verts, int num_loops, int num_polys)
{
	DerivedMesh *dm = CDDM_new(num_verts, 0, 0, num_loops, num_polys);

	export_data->dm = dm;
	export_data->mvert = dm->getVertArray(dm);
	export_data->mloop = dm->getLoopArray(dm);
	export_data->mpoly = dm->getPolyArray(dm);
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
	MPoly *mpoly = &export_data->mpoly[poly_index];

	BLI_assert(poly_index >= 0 && poly_index < dm->getNumPolys(dm));
	BLI_assert(start_loop >= 0 && start_loop <= dm->getNumLoops(dm) - num_loops);
	BLI_assert(num_loops >= 3);

	/* Set original index of the poly. */
	if (export_data->poly_origindex) {
		export_data->poly_origindex[poly_index] = ORIGINDEX_NONE;
	}

	/* Set poly data itself. */
	mpoly->loopstart = start_loop;
	mpoly->totloop = num_loops;
	mpoly->flag |= ME_SMOOTH;
}

/* Set list vertex and edge which are adjucent to loop with given index. */
static void exporter_SetLoop(ExportMeshData *export_data, int loop_index, int vertex)
{
	DerivedMesh *dm = export_data->dm;
	MLoop *mloop = &export_data->mloop[loop_index];

	BLI_assert(loop_index >= 0 && loop_index < dm->getNumLoops(dm));
	BLI_assert(vertex >= 0 && vertex < dm->getNumVerts(dm));

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

static struct VDBMeshDescr *vdb_mesh_from_dm(Object *object,
											 DerivedMesh *dm,
											 const DMArrays *dm_arrays)
{
	ImportMeshData import_data;

	import_data.dm = dm;
	copy_m4_m4(import_data.obmat, object->obmat);
	import_data.mvert = dm_arrays->mvert;
	import_data.mloop = dm_arrays->mloop;
	import_data.mpoly = dm_arrays->mpoly;

	return VDB_addMesh(&import_data, &MeshImporter);
}

static void populate_particle_list(ParticleMesherModifierData *pmmd, Scene *scene, Object *ob)
{
	ParticleSystem *psys = pmmd->psys;

	if (psys) {
		ParticleSimulationData sim;
		ParticleKey state;
		int p;

		if (psys->part->size > 0.0f) {
//			part_list.has_radius(true);
		}

		/* TODO(kevin): this isn't right at all */
		if (psys->part->normfac > 0.0f) {
//			part_list.has_velocity(true);
		}

		sim.scene = scene;
		sim.ob = ob;
		sim.psys = psys;

		psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

		for (p = 0; p < psys->totpart; p++) {
			float pos[3], vel[3];

			if (psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
				continue;
			}

			state.time = BKE_scene_frame_get(scene);

			if (psys_get_particle_state(&sim, p, &state, 0) == 0) {
				continue;
			}

			/* location */
			copy_v3_v3(pos, state.co);

			/* velocity */
			sub_v3_v3v3(vel, state.co, psys->particles[p].prev_state.co);

			mul_v3_fl(vel, psys->part->normfac);

			OpenVDB_add_particle(pmmd->part_list,
			                     pmmd->level_set,
			                     pos,
			                     psys->particles[p].size,
			                     vel);
		}

		if (psys->lattice_deform_data) {
			end_latt_deform(psys->lattice_deform_data);
			psys->lattice_deform_data = NULL;
		}
	}
}

DerivedMesh *NewParticleDerivedMesh(DerivedMesh *dm, struct Object *ob,
									DerivedMesh *cutter_dm, struct Object *cutter_ob,
									ParticleMesherModifierData *pmmd, Scene *scene)
{

	struct VDBMeshDescr *mask_mesh = NULL, *output = NULL;
	struct OpenVDBPrimitive *filter_mask = NULL, *mesher_mask = NULL;
	LevelSetFilter *filter = NULL;
	DerivedMesh *output_dm = NULL;
	DMArrays dm_arrays, cutter_dm_arrays;
//	bool result;

	if (dm == NULL) {
		return NULL;
	}

	dm_arrays_get(dm, &dm_arrays);

	if (cutter_dm != NULL) {
		dm_arrays_get(cutter_dm, &cutter_dm_arrays);

		mask_mesh = vdb_mesh_from_dm(cutter_ob, cutter_dm, &cutter_dm_arrays);
	}

	pmmd->level_set = create_level_set(pmmd->voxel_size, pmmd->half_width);

	/* Generate a particle list */

	pmmd->part_list = OpenVDB_create_part_list(pmmd->psys->totpart,
	                                           pmmd->part_scale_factor,
	                                           pmmd->part_vel_factor);

	populate_particle_list(pmmd, scene, ob);

	filter_mask = create_level_set(pmmd->voxel_size, pmmd->half_width);

	/* Create a level set from the particle list */

	OpenVDB_from_particles(pmmd->level_set, filter_mask, pmmd->part_list,
	                       pmmd->generate_mask, pmmd->mask_width, pmmd->min_part_radius,
	                       pmmd->generate_trails, pmmd->trail_size);

	/* Apply some filters to the level set */
	for (filter = pmmd->filters.first; filter; filter = filter->next) {
		if (!(filter->flag & LVLSETFILTER_MUTE)) {
			OpenVDB_filter_level_set(pmmd->level_set, filter_mask,
			                         filter->accuracy, filter->type,
			                         filter->iterations, filter->width,
			                         filter->offset);
		}
	}

	/* Convert the level set to a mesh and output it */

	if (mask_mesh) {
		mesher_mask = OpenVDB_from_polygons(mask_mesh, pmmd->voxel_size, pmmd->int_band, pmmd->ext_band);
	}

	output = OpenVDB_to_polygons(pmmd->level_set, mesher_mask, pmmd->isovalue, pmmd->adaptivity, pmmd->mask_offset, pmmd->invert_mask);

	VDB_deleteMesh(mask_mesh);

	if (true) {
		ExportMeshData export_data;

		invert_m4_m4(export_data.obimat, ob->obmat);

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
