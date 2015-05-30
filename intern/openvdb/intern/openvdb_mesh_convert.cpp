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
#include "openvdb_primitive.h"

using namespace openvdb;

namespace internal {

/* TODO(kevin): - figure out how/when to use tools::MeshToVoxelEdgeData, would
 *				probably need vertex normals.
 *				- handle case where we'd want a fog volume instead of a plain
 *				level set.
 */
OpenVDBPrimitive *OpenVDB_from_polygons(OpenVDBGeom *geom,
                                        OpenVDBPrimitive *input_prim,
                                        float voxel_size, float int_band, float ext_band)
{
	math::Transform::Ptr transform;

	if (input_prim) {
		transform = input_prim->getGridPtr()->transform().copy();
	}
	else {
		transform = math::Transform::createLinearTransform(voxel_size);
	}

	/* Convert vertices to the grid's index space */
	std::vector<openvdb::Vec3s> points;
	const size_t num_points = geom->numPoints();
	points.reserve(num_points);

	for (size_t i = 0; i < num_points; ++i) {
		openvdb::Vec3s vert = geom->point(i);
		points.push_back(transform->worldToIndex(vert));
	}

//	int_band /= voxel_size;
//	ext_band /= voxel_size;

	tools::MeshToVolume<FloatGrid> voxelizer(transform);
	voxelizer.convertToLevelSet(points, geom->getPolys(), int_band, ext_band);

	FloatGrid::Ptr grid = voxelizer.distGridPtr();

	/* XXX - should be done in a better way */
	if (input_prim) {
		delete input_prim;
	}

	OpenVDBPrimitive *vdb_prim = new OpenVDBPrimitive();
	vdb_prim->setGrid(grid);

	io::File file("/home/kevin/parts2.vdb");
	file.setCompression(io::COMPRESS_ACTIVE_MASK | io::COMPRESS_ZIP);
	GridPtrVec grids;
	grids.push_back(grid);
	file.write(grids);
	file.close();

	return vdb_prim;
}

OpenVDBGeom *OpenVDB_to_polygons(OpenVDBPrimitive *level_set,
                                 OpenVDBPrimitive *mask_grid,
                                 float isovalue, float adaptivity,
                                 float mask_offset, bool invert_mask)
{
	FloatGrid::Ptr ls_grid = gridPtrCast<FloatGrid>(level_set->getGridPtr());
	math::Transform::Ptr transform = ls_grid->transformPtr();

	tools::VolumeToMesh mesher(isovalue, adaptivity);

	if (mask_grid) {
		FloatGrid::ConstPtr grid = gridConstPtrCast<FloatGrid>(mask_grid->getConstGridPtr());
		mesher.setSurfaceMask(tools::sdfInteriorMask(*grid, mask_offset),
		                      invert_mask);
	}

	mesher(*ls_grid);

	const tools::PointList &points = mesher.pointList();
	tools::PolygonPoolList &polygonPoolList = mesher.polygonPoolList();

	OpenVDBGeom *geom = new OpenVDBGeom(mesher.pointListSize(), 0);

	for (size_t i = 0; i < mesher.pointListSize(); ++i) {
		geom->addPoint(transform->indexToWorld(points[i]));
	}

	for (size_t i = 0; i < mesher.polygonPoolListSize(); ++i) {
		tools::PolygonPool &polygons = polygonPoolList[i];

		for (size_t j = 0; j < polygons.numQuads(); ++j) {
			geom->addQuad(polygons.quad(j));
		}

		for (size_t j = 0; j < polygons.numTriangles(); ++j) {
			geom->addTriangle(polygons.triangle(j));
		}
	}

	return geom;
}

}
