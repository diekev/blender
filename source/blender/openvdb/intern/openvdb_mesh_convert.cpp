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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>

#include "openvdb_capi.h"
#include "openvdb_intern.h"

using namespace openvdb;

/* TODO(kevin): - figure out how/when to use tools::MeshToVoxelEdgeData, would
 *				probably need vertex normals.
 *				- handle case where we'd want a density grid instead of a plain
 *				level set (tools::sdfToFogVolume).
 */
FloatGrid::Ptr OpenVDB_from_polygons(VDBMeshDescr *mesh_descr,
									 const MeshToVolumeSettings &settings,
									 math::Transform::Ptr transform)
{
	tools::MeshToVolume<FloatGrid> voxelizer(transform);

	voxelizer.convertToLevelSet(mesh_descr->points, mesh_descr->polys,
								settings.ext_band, settings.int_band);

	FloatGrid::Ptr grid = voxelizer.distGridPtr();

	io::File file("/home/kevin/parts2.vdb");
	GridPtrVec grids;
	grids.push_back(grid);
	file.write(grids);
	file.close();

	return grid;
}

struct VDBMeshDescr *OpenVDB_to_polygons(FloatGrid::Ptr level_set,
										 FloatGrid::Ptr mask_grid,
										 math::Transform::Ptr transform,
										 const VolumeToMeshSettings &settings)
{
	VDBMeshDescr *mesh_descr = new VDBMeshDescr;

	tools::VolumeToMesh mesher(settings.isovalue, settings.adaptivity);

	if (mask_grid) {
		std::cout << "Generating surface mask\n";
		FloatGrid::ConstPtr grid = gridConstPtrCast<FloatGrid>(mask_grid);
		mesher.setSurfaceMask(tools::sdfInteriorMask(*grid, settings.mask_offset),
							  settings.invert_mask);
	}

	mesher(*level_set);

	const tools::PointList &points = mesher.pointList();
	tools::PolygonPoolList &polygonPoolList = mesher.polygonPoolList();

	mesh_descr->points.reserve(mesher.pointListSize());
	for (size_t i = 0; i < mesher.pointListSize(); ++i) {
		mesh_descr->points.push_back(transform->indexToWorld(points[i]));
	}

	for (size_t i = 0; i < mesher.polygonPoolListSize(); ++i) {
		tools::PolygonPool &polygons = polygonPoolList[i];

		for (size_t j = 0; j < polygons.numQuads(); ++j) {
			mesh_descr->polys.push_back(polygons.quad(j));
		}

		for (size_t j = 0; j < polygons.numTriangles(); ++j) {
			mesh_descr->tris.push_back(polygons.triangle(j));
		}
	}

	return mesh_descr;
}
