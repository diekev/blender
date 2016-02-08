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

#include "openvdb_render_capi.h"

#include "../guardedalloc/MEM_guardedalloc.h"

#include "intern/openvdb_primitive.h"
#include "intern/openvdb_process_utils.h"
#include "intern/openvdb_render_utils.h"

struct OpenVDBInternalNode2 { int unused; };

float *OpenVDB_get_texture_buffer(struct OpenVDBPrimitive *prim,
                                  int res[3], float bbmin[3], float bbmax[3])
{
	const int storage = internal::get_grid_storage(prim->getGrid());

	internal::DenseTextureResOp res_op(res, bbmin, bbmax);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, res_op);

	const int numcells = res[0] * res[1] * res[2];

	if (numcells == 0) {
		return NULL;
	}

	float *buffer = (float *)MEM_mallocN(numcells * sizeof(float), "OpenVDB texture buffer");

	internal::DenseTextureOp op(buffer);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, op);

	return buffer;
}

void OpenVDB_get_draw_buffers_nodes(OpenVDBPrimitive *prim,
                                    float (**r_verts)[3], float (**r_colors)[3], int *r_numverts)
{
	const int min_level = 0;
	const int max_level = 3;

	const int storage = internal::get_grid_storage(prim->getGrid());

	internal::CellsBufferSizeOp size_op(r_numverts, min_level, max_level, false);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, size_op);

	*r_verts = (float (*)[3])MEM_mallocN((*r_numverts) * sizeof(float) * 3, "OpenVDB vertex buffer");
	*r_colors = (float (*)[3])MEM_mallocN((*r_numverts) * sizeof(float) * 3, "OpenVDB color buffer");

	internal::CellsBufferOp op(min_level, max_level, false, *r_verts, *r_colors);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, op);
}

void OpenVDB_get_draw_buffers_boxes(OpenVDBPrimitive *prim, float value_scale,
                                    float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts)
{
	const int storage = internal::get_grid_storage(prim->getGrid());

	internal::BoxesBufferSizeOp size_op(r_numverts);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, size_op);

	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3;
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer");
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer");
	*r_normals = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB normal buffer");

	internal::BoxesBufferOp op(value_scale, *r_verts, *r_colors, *r_normals);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, op);
}

void OpenVDB_get_draw_buffers_needles(OpenVDBPrimitive *prim, float value_scale,
                                      float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts)
{
	const int storage = internal::get_grid_storage(prim->getGrid());

	internal::NeedlesBufferSizeOp size_op(r_numverts);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, size_op);

	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3;
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer");
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer");
	*r_normals = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB normal buffer");

	internal::NeedlesBufferOp op(value_scale, *r_verts, *r_colors, *r_normals);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, op);
}

void OpenVDB_get_draw_buffers_staggered(OpenVDBPrimitive *prim, float value_scale,
                                        float (**r_verts)[3], float (**r_colors)[3], int *r_numverts)
{
	const int storage = internal::get_grid_storage(prim->getGrid());

	internal::StaggeredBufferSizeOp size_op(r_numverts);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, size_op);

	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3;
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer");
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer");

	internal::StaggeredBufferOp op(value_scale, *r_verts, *r_colors);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, op);
}

void OpenVDB_get_value_range(struct OpenVDBPrimitive *prim, float *bg, float *min, float *max)
{
	const int storage = internal::get_grid_storage(prim->getGrid());

	internal::GridValueRangeOp op(min, max, bg);
	internal::process_typed_grid(prim->getConstGridPtr(), storage, op);
}

void OpenVDB_get_internal_nodes_count(OpenVDBPrimitive *prim, int **r_nodes_counts,
                                      int *r_num_atlas, const OpenVDBInternalNode2 ***r_nodes_handles,
                                      int *r_num_nodes)
{
	using namespace openvdb;

	typedef          FloatTree::RootNodeType RootNodeType;
	typedef typename RootNodeType::ChildNodeType Int1NodeType;
	typedef typename Int1NodeType::ChildNodeType Int2NodeType;

	FloatGrid::Ptr grid = openvdb::gridPtrCast<FloatGrid>(prim->getGridPtr());

	std::vector<int> nodes_counts;
	std::vector<const Int2NodeType *> nodes;

	FloatTree::NodeCIter node_iter = grid->tree().cbeginNode();

	for (; node_iter; ++node_iter) {
		const int depth = node_iter.getDepth();

		if (depth != 2)
			continue;

		int node_count = 0;
		int leaf_count = 0;

		for (; node_iter; ++node_iter) {
			const int depth = node_iter.getDepth();

			if (depth != 2)
				continue;

			FloatTree::NodeCIter prev_it = node_iter;
			node_iter = ++node_iter;

			const Int2NodeType *node = NULL;
			node_iter.getNode(node);
			assert(node);

			for (typename Int2NodeType::ChildOnCIter iter = node->cbeginChildOn();
			     iter;
			     ++iter)
			{
				++leaf_count;
			}

			if (leaf_count >= 4096) {
				node_iter = prev_it;
				break;
			}

			nodes.push_back(node);
			++node_count;
		}

		nodes_counts.push_back(node_count);
	}

	*r_num_atlas = nodes_counts.size();
	*r_num_nodes = nodes.size();
	*r_nodes_counts = (int *)MEM_mallocN(sizeof(int) * (*r_num_atlas), "OpenVDB node counts");
	*r_nodes_handles = (const OpenVDBInternalNode2 **)MEM_mallocN(sizeof(OpenVDBInternalNode2 *) * (*r_num_nodes), "OpenVDB node counts");

	for (size_t i = 0; i < nodes_counts.size(); ++i) {
		(*r_nodes_counts)[i] = nodes_counts[i];
	}

	for (size_t i = 0; i < nodes.size(); ++i) {
		(*r_nodes_handles)[i] = reinterpret_cast<const OpenVDBInternalNode2 *>(nodes[i]);
	}
}

void OpenVDB_get_leaf_buffers(const OpenVDBInternalNode2 *node_handle,
                              OpenVDBLeafNodeBufferCb cb,
                              void *userdata)
{
	typedef openvdb::FloatTree::RootNodeType     RootNodeType;
	typedef typename RootNodeType::ChildNodeType Int1NodeType;
	typedef typename Int1NodeType::ChildNodeType Int2NodeType;

	const Int2NodeType *node = reinterpret_cast<const Int2NodeType *>(node_handle);

	for (typename Int2NodeType::ChildOnCIter iter = node->cbeginChildOn();
	     iter;
	     ++iter)
	{
		openvdb::FloatTree::LeafNodeType leaf = *iter;
		cb(userdata, leaf.buffer().data());
	}
}

void OpenVDB_get_node_bounds(const OpenVDBPrimitive *prim,
                             const OpenVDBInternalNode2 *node_handle,
                             float bbmin[3], float bbmax[3])
{
	openvdb::FloatGrid::ConstPtr grid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(prim->getConstGridPtr());

	typedef openvdb::FloatTree::RootNodeType     RootNodeType;
	typedef typename RootNodeType::ChildNodeType Int1NodeType;
	typedef typename Int1NodeType::ChildNodeType Int2NodeType;

	const Int2NodeType *node = reinterpret_cast<const Int2NodeType *>(node_handle);

	const openvdb::math::CoordBBox bbox = node->getNodeBoundingBox();

	const openvdb::Vec3f min = bbox.min().asVec3s() - openvdb::Vec3f(0.5f, 0.5f, 0.5f);
	const openvdb::Vec3f max = bbox.max().asVec3s() + openvdb::Vec3f(0.5f, 0.5f, 0.5f);

	const openvdb::Vec3f wmin = grid->indexToWorld(min);
	const openvdb::Vec3f wmax = grid->indexToWorld(max);

	wmin.toV(bbmin);
	wmax.toV(bbmax);
}

void OpenVDB_node_get_leaf_indices(const OpenVDBInternalNode2 *node_handle,
                                   int **indirection_map, int *leaf_index, int internal_node_index)
{
	typedef openvdb::FloatTree::RootNodeType     RootNodeType;
	typedef typename RootNodeType::ChildNodeType Int1NodeType;
	typedef typename Int1NodeType::ChildNodeType Int2NodeType;

	const Int2NodeType *node = reinterpret_cast<const Int2NodeType *>(node_handle);

	for (typename Int2NodeType::ChildOnCIter iter = node->cbeginChildOn();
	     iter;
	     ++iter)
	{
		int index = internal_node_index * 4096 + iter.offset();
		(*indirection_map)[index] = *leaf_index;
		(*leaf_index)++;
	}
}
