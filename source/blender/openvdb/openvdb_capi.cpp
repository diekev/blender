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

#include <openvdb/util/Util.h>  /* for openvdb::util::INVALID_IDX */
#include <vector>

extern "C" {
#include "BKE_DerivedMesh.h"

#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
}

#include "openvdb_capi.h"
#include "openvdb_intern.h"
#include "particlelist.h"

using namespace openvdb;

int OpenVDB_getVersionHex()
{
	return OPENVDB_LIBRARY_VERSION;
}

/* TODO(kevin): these templates aren't that nice... */
template<typename Settings>
LevelSetFilterSettings get_level_set_filter_settings(Settings *settings)
{
	LevelSetFilterSettings lsfs;

	lsfs.accuracy = settings->accuracy;
	lsfs.iterations = settings->iterations;
	lsfs.type = settings->type;
	lsfs.width = settings->width;
	lsfs.offset = settings->offset;

	return lsfs;
}

template<typename Settings>
MeshToVolumeSettings get_mesh_to_volume_settings(Settings *settings)
{
	MeshToVolumeSettings mtvs;

	mtvs.ext_band = settings->ext_band;
	mtvs.int_band = settings->int_band;

	return mtvs;
}

template<typename Settings>
ParticlesToLevelSetSettings get_particles_level_set_settings(Settings *settings)
{
	ParticlesToLevelSetSettings ptlss;

	ptlss.voxel_size = settings->voxel_size;
	ptlss.min_part_radius = settings->min_part_radius;
	ptlss.half_width = settings->half_width;
	ptlss.part_scale_factor = settings->part_scale_factor;
	ptlss.part_vel_factor = settings->part_vel_factor;
	ptlss.trail_size = settings->trail_size;
	ptlss.mask_width = settings->mask_width;
	ptlss.generate_mask = settings->generate_mask;
	ptlss.generate_trails = settings->generate_trails;

	return ptlss;
}

template<typename Settings>
VolumeToMeshSettings get_volume_to_mesh_settings(Settings *settings)
{
	VolumeToMeshSettings vtms;

	vtms.isovalue = settings->isovalue;
	vtms.adaptivity = settings->adaptivity;
	vtms.mask_offset = settings->mask_offset;
	vtms.invert_mask = settings->invert_mask;

	return vtms;
}

VDBMeshDescr *VDB_addMesh(struct ImportMeshData *import_data,
						  VDBMeshImporter *mesh_importer)
{
	VDBMeshDescr *mesh_descr = new VDBMeshDescr;

	const int num_verts = mesh_importer->getNumVerts(import_data);
	mesh_descr->points.reserve(num_verts);

	for (int i = 0; i < num_verts; ++i) {
		float position[3];

		mesh_importer->getVertCoord(import_data, i, position);

		openvdb::Vec3s vertex(position[0], position[1], position[2]);

		mesh_descr->points.push_back(vertex);
	}

	const int num_polys = mesh_importer->getNumPolys(import_data);
	mesh_descr->polys.reserve(num_polys);

#define MAX_STATIC_VERTS 64

	int verts_of_poly_static[MAX_STATIC_VERTS];
	int *verts_of_poly_dynamic = NULL;
	int verts_of_poly_dynamic_size = 0;

	for (int i = 0; i < num_polys; ++i) {
		int verts_per_poly = mesh_importer->getPolyNumVerts(import_data, i);
		int *verts_of_poly;

		if (verts_per_poly <= MAX_STATIC_VERTS) {
			verts_of_poly = verts_of_poly_static;
		}
		else {
			if (verts_of_poly_dynamic_size < verts_per_poly) {
				if (verts_of_poly_dynamic != NULL) {
					delete [] verts_of_poly_dynamic;
				}
				verts_of_poly_dynamic = new int[verts_per_poly];
				verts_of_poly_dynamic_size = verts_per_poly;
			}
			verts_of_poly = verts_of_poly_dynamic;
		}

		/* We assume every face is a quad, and we initialize all the indices to
		 * some INVALID_IDX value. OpenVDB will detect if we have a triangle
		 * based on whether or not the last index is this INVALID_IDX value.
		 * Easier this way than adding a check in the loop below.
		 */
		openvdb::Vec4I face(openvdb::util::INVALID_IDX);

		mesh_importer->getPolyVerts(import_data, i, verts_of_poly);

		for (int j = 0; j < verts_per_poly; ++j) {
			face[j] = verts_of_poly[j];
		}

		mesh_descr->polys.push_back(face);
	}

	if (verts_of_poly_dynamic != NULL) {
		delete [] verts_of_poly_dynamic;
	}

	return mesh_descr;
#undef MAX_STATIC_VERTS
}

void VDB_exportMesh(VDBMeshDescr *mesh_descr,
					VDBMeshExporter *mesh_exporter,
					struct ExportMeshData *export_data)
{
	const int num_verts = mesh_descr->points.size();
	const int num_loops = mesh_descr->polys.size() * 4 + mesh_descr->tris.size() * 3;
	const int num_polys = mesh_descr->polys.size() + mesh_descr->tris.size();

	// Initialize arrays for geometry in exported mesh.
	mesh_exporter->initGeomArrays(export_data, num_verts, num_loops, num_polys);

	// Export all the vertices.
	for (int i = 0; i < num_verts; ++i) {
		openvdb::Vec3s vertex = mesh_descr->points[i];

		float coord[3] = { vertex.x(), vertex.y(), vertex.z() };

		mesh_exporter->setVert(export_data, i, coord);
	}

	int loop_index = 0, poly_index = 0;

	// Export all the quads.
	for (unsigned int i = 0; i < mesh_descr->polys.size(); ++i, ++poly_index) {
		int start_loop_index = loop_index;
		openvdb::Vec4I poly = mesh_descr->polys[i];

		for (int j = 0; j < 4; ++j) {
			mesh_exporter->setLoop(export_data, loop_index++, poly[j]);
		}

		mesh_exporter->setPoly(export_data, poly_index, start_loop_index, 4);
	}

	// Export all the tris, if adaptive meshing.
	for (unsigned int i = 0; i < mesh_descr->tris.size(); ++i, ++poly_index) {
		int start_loop_index = loop_index;
		openvdb::Vec3I triangle = mesh_descr->tris[i];

		for (int j = 0; j < 3; ++j) {
			mesh_exporter->setLoop(export_data, loop_index++, triangle[j]);
		}

		mesh_exporter->setPoly(export_data, poly_index, start_loop_index, 3);
	}
}

void VDB_deleteMesh(VDBMeshDescr *mesh_descr)
{
	delete mesh_descr;
}

bool OpenVDB_performParticleSurfacing(Scene *scene, Object *ob,
									  ParticleMesherModifierData *pmmd,
									  VDBMeshDescr *mask_mesh,
									  VDBMeshDescr **output_mesh)
{
	/* Init OpenVDB lib */
	initialize();

	*output_mesh = NULL;
	VDBMeshDescr *output_descr = NULL;

	try {
		math::Transform::Ptr transform = math::Transform::createLinearTransform(pmmd->voxel_size);

		FloatGrid::Ptr level_set = createLevelSet<FloatGrid>(pmmd->voxel_size, pmmd->half_width);
		FloatGrid::Ptr filter_mask = createLevelSet<FloatGrid>(pmmd->voxel_size, pmmd->half_width);

		/* Convert the particles to a level set */

		ParticleList part_list = create_particle_list(scene, ob, pmmd->psys, transform,
													  pmmd->part_scale_factor,
													  pmmd->part_vel_factor);

		ParticlesToLevelSetSettings part_settings = get_particles_level_set_settings(pmmd);

		OpenVDB_from_particles(level_set, filter_mask, part_settings, part_list);

		/* Apply some filters to the level set */

		LevelSetFilter *mod_filter = (LevelSetFilter *)pmmd->filters.first;
		std::vector<LevelSetFilterSettings> filter_settings_list;

		while (mod_filter) {
			if (!(mod_filter->flag & LVLSETFILTER_MUTE)) {
				LevelSetFilterSettings filter_settings = get_level_set_filter_settings(mod_filter);
				filter_settings_list.push_back(filter_settings);
			}
			mod_filter = mod_filter->next;
		}

		OpenVDB_filter_level_set(level_set, filter_mask, filter_settings_list);

		/* Convert the level set to a mesh and output it */
		FloatGrid::Ptr mesher_mask;

		if (mask_mesh) {
			MeshToVolumeSettings voxelizer_settings = get_mesh_to_volume_settings(pmmd);

			mesher_mask = OpenVDB_from_polygons(mask_mesh, voxelizer_settings, transform);
		}

		VolumeToMeshSettings mesher_settings = get_volume_to_mesh_settings(pmmd);

		output_descr = OpenVDB_to_polygons(level_set, mesher_mask, transform, mesher_settings);
	}
	catch (std::exception &e) {
		std::cerr << "OpenVDB exception " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "Unknown error in OpenVDB library" << std::endl;
	}

	*output_mesh = output_descr;

	return output_descr != NULL;
}
