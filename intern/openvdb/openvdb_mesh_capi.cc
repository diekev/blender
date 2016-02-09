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

#include "openvdb_mesh_capi.h"

#include "intern/openvdb_dense_convert.h"
#include "intern/openvdb_primitive.h"

static void get_mesh_geometry(OpenVDBMeshIterator *it, float mat[4][4],
                              std::vector<openvdb::math::Vec3s> &vertices,
                              std::vector<openvdb::Vec3I> &triangles)
{
	using openvdb::math::Vec3s;
	using openvdb::Vec3I;
	using openvdb::Vec4I;
	using openvdb::Mat4R;

	Mat4R M = internal::convertMatrix(mat);

	for (; it->has_vertices(it); it->next_vertex(it)) {
		float co[3];
		it->get_vertex(it, co);
		Vec3s v(co[0], co[1], co[2]);
		vertices.push_back(M.transform(v));
	}

	for (; it->has_triangles(it); it->next_triangle(it)) {
		int a, b, c;
		it->get_triangle(it, &a, &b, &c);
		assert(a < vertices.size() && b < vertices.size() && c < vertices.size());
		triangles.push_back(Vec3I(a, b, c));
	}
}

OpenVDBPrimitive *OpenVDBPrimitive_from_mesh(OpenVDBMeshIterator *it, float mat[4][4], const float voxel_size)
{
	using namespace openvdb;

	std::vector<openvdb::math::Vec3s> vertices;
	std::vector<openvdb::Vec3I> triangles;

	get_mesh_geometry(it, mat, vertices, triangles);

	const float bandwidth_ex = static_cast<float>(LEVEL_SET_HALF_WIDTH);
	const float bandwidth_in = static_cast<float>(LEVEL_SET_HALF_WIDTH);

	math::Transform::Ptr cell_transform = math::Transform::createLinearTransform(voxel_size);

	FloatGrid::ConstPtr grid = tools::meshToSignedDistanceField<FloatGrid>(
	                               *cell_transform, vertices, triangles,
	                               std::vector<Vec4I>(), bandwidth_ex, bandwidth_in);

	OpenVDBPrimitive *prim = new OpenVDBPrimitive();
	prim->setGrid(grid);

	return prim;
}
