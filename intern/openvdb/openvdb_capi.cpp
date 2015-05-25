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
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
}

#include "openvdb_capi.h"
#include "openvdb_intern.h"
#include "openvdb_primitive.h"
#include "particlelist.h"

using namespace openvdb;

int OpenVDB_getVersionHex()
{
	return OPENVDB_LIBRARY_VERSION;
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

	/* Initialize arrays for geometry in exported mesh. */
	mesh_exporter->initGeomArrays(export_data, num_verts, num_loops, num_polys);

	/* Export all the vertices. */
	for (int i = 0; i < num_verts; ++i) {
		openvdb::Vec3s vertex = mesh_descr->points[i];

		float coord[3] = { vertex.x(), vertex.y(), vertex.z() };

		mesh_exporter->setVert(export_data, i, coord);
	}

	int loop_index = 0, poly_index = 0;

	/* Export all the quads. */
	for (unsigned int i = 0; i < mesh_descr->polys.size(); ++i, ++poly_index) {
		int start_loop_index = loop_index;
		openvdb::Vec4I poly = mesh_descr->polys[i];

		for (int j = 0; j < 4; ++j) {
			mesh_exporter->setLoop(export_data, loop_index++, poly[j]);
		}

		mesh_exporter->setPoly(export_data, poly_index, start_loop_index, 4);
	}

	/* Export all the tris, if adaptive meshing. */
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

/* ****************************** Particle List ****************************** */

ParticleList *OpenVDB_create_part_list(size_t totpart, float rad_scale, float vel_scale)
{
	return new ParticleList(totpart, rad_scale, vel_scale);
}

void OpenVDB_part_list_free(ParticleList *part_list)
{
	delete part_list;
	part_list = NULL;
}

void OpenVDB_add_particle(ParticleList *part_list, OpenVDBPrimitive *vdb_prim,
                          float pos[3], float rad, float vel[3])
{
	openvdb::Vec3R nvel(vel), npos(pos);
	float nrad = rad * part_list->radius_scale();
	nvel *= part_list->velocity_scale();

	if (vdb_prim) {
		npos = vdb_prim->getGrid().transform().worldToIndex(npos);
	}

	part_list->add(npos, nrad, nvel);
}

void OpenVDB_from_particles(OpenVDBPrimitive *level_set, OpenVDBPrimitive *mask_grid,
                            ParticleList *Pa, bool mask, float mask_width,
                            float min_radius, bool trail, float trail_size)
{
	internal::OpenVDB_from_particles(level_set, mask_grid, *Pa, mask, mask_width,
	                                 min_radius, trail, trail_size);
}

/* **************************** OpenVDB Primitive **************************** */

OpenVDBPrimitive *create(int grid_type)
{
	OpenVDBPrimitive *vdb_prim = new OpenVDBPrimitive();
	openvdb::GridBase::Ptr grid;

	switch (grid_type) {
		case VDB_GRID_FLOAT:
			grid = openvdb::FloatGrid::create(0.0f);
			break;
		case VDB_GRID_DOUBLE:
			grid = openvdb::DoubleGrid::create(0.0);
			break;
		case VDB_GRID_INT32:
			grid = openvdb::Int32Grid::create(0);
			break;
		case VDB_GRID_INT64:
			grid = openvdb::Int64Grid::create(0);
			break;
		case VDB_GRID_BOOL:
			grid = openvdb::BoolGrid::create(false);
			break;
		case VDB_GRID_VEC3F:
			grid = openvdb::Vec3fGrid::create(openvdb::Vec3f(0.0f));
			break;
		case VDB_GRID_VEC3D:
			grid = openvdb::Vec3dGrid::create(openvdb::Vec3d(0.0));
			break;
		case VDB_GRID_VEC3I:
			grid = openvdb::Vec3IGrid::create(openvdb::Vec3I(0));
			break;
	}

	vdb_prim->setGrid(grid);

	return vdb_prim;
}

OpenVDBPrimitive *create_level_set(float voxel_size, float half_width)
{
	using namespace openvdb;

	OpenVDBPrimitive *vdb_prim = new OpenVDBPrimitive();

	FloatGrid::Ptr grid = createLevelSet<FloatGrid>(voxel_size, half_width);
	grid->setTransform(math::Transform::createLinearTransform(voxel_size));
	vdb_prim->setGrid(grid);

	return vdb_prim;
}

void OpenVDBPrimitive_free(OpenVDBPrimitive *vdb_prim)
{
	delete vdb_prim;
	vdb_prim = NULL;
}

void OpenVDB_filter_level_set(OpenVDBPrimitive *level_set, OpenVDBPrimitive *filter_mask,
                              int accuracy, int type, int iterations, int width, float offset)
{
	internal::OpenVDB_filter_level_set(level_set, filter_mask, accuracy, type, iterations, width, offset);
}


OpenVDBPrimitive *OpenVDB_from_polygons(VDBMeshDescr *dm, float voxel_size, float int_band, float ext_band)
{
	return internal::OpenVDB_from_polygons(dm, voxel_size, int_band, ext_band);
}


VDBMeshDescr *OpenVDB_to_polygons(OpenVDBPrimitive *level_set, OpenVDBPrimitive *mask_grid, float isovalue, float adaptivity, float mask_offset, bool invert_mask)
{
	return internal::OpenVDB_to_polygons(level_set, mask_grid, isovalue, adaptivity, mask_offset, invert_mask);
}
