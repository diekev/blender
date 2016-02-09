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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct OpenVDBMeshIterator;
struct OpenVDBPrimitive;

/* generic iterator for reading mesh data from unknown sources */

typedef bool (*OpenVDBMeshHasVerticesFn)(struct OpenVDBMeshIterator *it);
typedef bool (*OpenVDBMeshHasTrianglesFn)(struct OpenVDBMeshIterator *it);
typedef void (*OpenVDBMeshNextVertexFn)(struct OpenVDBMeshIterator *it);
typedef void (*OpenVDBMeshNextTriangleFn)(struct OpenVDBMeshIterator *it);
typedef void (*OpenVDBMeshGetVertexFn)(struct OpenVDBMeshIterator *it, float co[3]);
typedef void (*OpenVDBMeshGetTriangleFn)(struct OpenVDBMeshIterator *it, int *a, int *b, int *c);

typedef struct OpenVDBMeshIterator {
	OpenVDBMeshHasVerticesFn has_vertices;
	OpenVDBMeshHasTrianglesFn has_triangles;

	OpenVDBMeshNextVertexFn next_vertex;
	OpenVDBMeshNextTriangleFn next_triangle;

	OpenVDBMeshGetVertexFn get_vertex;
	OpenVDBMeshGetTriangleFn get_triangle;
} OpenVDBMeshIterator;

struct OpenVDBPrimitive *OpenVDBPrimitive_from_mesh(struct OpenVDBMeshIterator *it, float mat[4][4], const float voxel_size);

#ifdef __cplusplus
}
#endif
