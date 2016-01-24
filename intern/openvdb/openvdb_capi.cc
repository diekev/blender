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

#include "openvdb_capi.h"
#include "openvdb_dense_convert.h"
#include "openvdb_primitive.h"
#include "openvdb_util.h"

#include "../guardedalloc/MEM_guardedalloc.h"

struct OpenVDBFloatGrid { int unused; };
struct OpenVDBIntGrid { int unused; };
struct OpenVDBVectorGrid { int unused; };
struct OpenVDBInternalNode2 { int unused; };

int OpenVDB_getVersionHex()
{
	return openvdb::OPENVDB_LIBRARY_VERSION;
}

void OpenVDB_get_grid_info(const char *filename, OpenVDBGridInfoCallback cb, void *userdata)
{
	Timer(__func__);

	using namespace openvdb;

	initialize();

	io::File file(filename);
	file.open();

	GridPtrVecPtr grids = file.getGrids();
	int grid_num = grids->size();

	for (size_t i = 0; i < grid_num; ++i) {
		GridBase::ConstPtr grid = (*grids)[i];

		Name name = grid->getName();
		Name value_type = grid->valueType();
		bool is_color = false;
		if (grid->getMetadata< TypedMetadata<bool> >("is_color"))
			is_color = grid->metaValue<bool>("is_color");

		OpenVDBPrimitive *prim = new OpenVDBPrimitive;
		prim->setGrid(grid);

		cb(userdata, name.c_str(), value_type.c_str(), is_color, prim);
	}
}


OpenVDBFloatGrid *OpenVDB_export_grid_fl(
        OpenVDBWriter *writer,
        const char *name, float *data,
        const int res[3], float matrix[4][4],
        OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using openvdb::FloatGrid;

	FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
	FloatGrid *grid = internal::OpenVDB_export_grid<FloatGrid>(
	        writer,
	        name,
	        data,
	        res,
	        matrix,
	        mask_grid);

	return reinterpret_cast<OpenVDBFloatGrid *>(grid);
}

OpenVDBIntGrid *OpenVDB_export_grid_ch(
        OpenVDBWriter *writer,
        const char *name, unsigned char *data,
        const int res[3], float matrix[4][4],
        OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using openvdb::FloatGrid;
	using openvdb::Int32Grid;

	FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
	Int32Grid *grid = internal::OpenVDB_export_grid<Int32Grid>(
	        writer,
	        name,
	        data,
	        res,
	        matrix,
	        mask_grid);

	return reinterpret_cast<OpenVDBIntGrid *>(grid);
}

OpenVDBVectorGrid *OpenVDB_export_grid_vec(
        struct OpenVDBWriter *writer,
        const char *name,
        const float *data_x, const float *data_y, const float *data_z,
        const int res[3], float matrix[4][4], short vec_type,
        const bool is_color, OpenVDBFloatGrid *mask)
{
	Timer(__func__);

	using openvdb::GridBase;
	using openvdb::FloatGrid;
	using openvdb::VecType;

	FloatGrid *mask_grid = reinterpret_cast<FloatGrid *>(mask);
	GridBase *grid = internal::OpenVDB_export_vector_grid(
	        writer,
	        name,
	        data_x,
	        data_y,
	        data_z,
	        res,
	        matrix,
	        static_cast<VecType>(vec_type),
	        is_color,
	        mask_grid);

	return reinterpret_cast<OpenVDBVectorGrid *>(grid);
}

void OpenVDB_import_grid_fl(
        OpenVDBReader *reader,
        const char *name, float **data,
        const int res[3])
{
	Timer(__func__);

	internal::OpenVDB_import_grid<openvdb::FloatGrid>(reader, name, data, res);
}

void OpenVDB_import_grid_ch(
        OpenVDBReader *reader,
        const char *name, unsigned char **data,
        const int res[3])
{
	internal::OpenVDB_import_grid<openvdb::Int32Grid>(reader, name, data, res);
}

void OpenVDB_import_grid_vec(
        struct OpenVDBReader *reader,
        const char *name,
        float **data_x, float **data_y, float **data_z,
        const int res[3])
{
	Timer(__func__);

	internal::OpenVDB_import_grid_vector(reader, name, data_x, data_y, data_z, res);
}

OpenVDBWriter *OpenVDBWriter_create()
{
	return new OpenVDBWriter();
}

void OpenVDBWriter_free(OpenVDBWriter *writer)
{
	delete writer;
}

void OpenVDBWriter_set_flags(OpenVDBWriter *writer, const int flag, const bool half)
{
	int compression_flags = openvdb::io::COMPRESS_ACTIVE_MASK;

#ifdef WITH_OPENVDB_BLOSC
	if (flag == 0) {
		compression_flags |= openvdb::io::COMPRESS_BLOSC;
	}
	else
#endif
	if (flag == 1) {
		compression_flags |= openvdb::io::COMPRESS_ZIP;
	}
	else {
		compression_flags = openvdb::io::COMPRESS_NONE;
	}

	writer->setFlags(compression_flags, half);
}

void OpenVDBWriter_add_meta_fl(OpenVDBWriter *writer, const char *name, const float value)
{
	writer->insertFloatMeta(name, value);
}

void OpenVDBWriter_add_meta_int(OpenVDBWriter *writer, const char *name, const int value)
{
	writer->insertIntMeta(name, value);
}

void OpenVDBWriter_add_meta_v3(OpenVDBWriter *writer, const char *name, const float value[3])
{
	writer->insertVec3sMeta(name, value);
}

void OpenVDBWriter_add_meta_v3_int(OpenVDBWriter *writer, const char *name, const int value[3])
{
	writer->insertVec3IMeta(name, value);
}

void OpenVDBWriter_add_meta_mat4(OpenVDBWriter *writer, const char *name, float value[4][4])
{
	writer->insertMat4sMeta(name, value);
}

void OpenVDBWriter_write(OpenVDBWriter *writer, const char *filename)
{
	writer->write(filename);
}

OpenVDBReader *OpenVDBReader_create()
{
	return new OpenVDBReader();
}

void OpenVDBReader_free(OpenVDBReader *reader)
{
	delete reader;
}

void OpenVDBReader_open(OpenVDBReader *reader, const char *filename)
{
	reader->open(filename);
}

void OpenVDBReader_get_meta_fl(OpenVDBReader *reader, const char *name, float *value)
{
	reader->floatMeta(name, *value);
}

void OpenVDBReader_get_meta_int(OpenVDBReader *reader, const char *name, int *value)
{
	reader->intMeta(name, *value);
}

void OpenVDBReader_get_meta_v3(OpenVDBReader *reader, const char *name, float value[3])
{
	reader->vec3sMeta(name, value);
}

void OpenVDBReader_get_meta_v3_int(OpenVDBReader *reader, const char *name, int value[3])
{
	reader->vec3IMeta(name, value);
}

void OpenVDBReader_get_meta_mat4(OpenVDBReader *reader, const char *name, float value[4][4])
{
	reader->mat4sMeta(name, value);
}

OpenVDBPrimitive *OpenVDBPrimitive_create()
{
	return new OpenVDBPrimitive();
}

void OpenVDBPrimitive_free(OpenVDBPrimitive *vdb_prim)
{
    delete vdb_prim;
}

float *OpenVDB_get_texture_buffer(struct OpenVDBPrimitive *prim,
                                  int res[3], float bbmin[3], float bbmax[3])
{

	openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(prim->getGridPtr());

	if (!internal::OpenVDB_get_dense_texture_res(grid.get(), res, bbmin, bbmax))
		return NULL;

	int numcells = res[0] * res[1] * res[2];
	float *buffer = (float *)MEM_mallocN(numcells * sizeof(float), "smoke VDB domain texture buffer");
	internal::OpenVDB_create_dense_texture(grid.get(), buffer);

	return buffer;
}

void OpenVDB_get_bounds(struct OpenVDBPrimitive *prim, float bbmin[3], float bbmax[3])
{
	openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(prim->getGridPtr());

	internal::OpenVDB_get_grid_bounds(grid.get(), bbmin, bbmax);
}

float OpenVDB_get_voxel_size(struct OpenVDBPrimitive *prim)
{
	return prim->getConstGrid().transform().voxelSize()[0];
}

void OpenVDB_get_draw_buffers_nodes(OpenVDBPrimitive *prim,
                                          float (**r_verts)[3], float (**r_colors)[3], int *r_numverts)
{
	const int min_level = 0;
	const int max_level = 3;

	openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(prim->getGridPtr());

	internal::OpenVDB_get_draw_buffer_size_cells(grid.get(), min_level, max_level, false, r_numverts);
	*r_verts = (float (*)[3])MEM_mallocN((*r_numverts) * sizeof(float) * 3, "OpenVDB vertex buffer");
	*r_colors = (float (*)[3])MEM_mallocN((*r_numverts) * sizeof(float) * 3, "OpenVDB color buffer");
	internal::OpenVDB_get_draw_buffers_cells(grid.get(), min_level, max_level, false, *r_verts, *r_colors);
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

#if 0
/* ------------------------------------------------------------------------- */
/* Drawing */

/* Helper macro for compact grid type selection code.
 * This allows conversion of a grid id enum to templated function calls.
 * DO_GRID must be defined locally.
 */
#define SELECT_SMOKE_GRID(data, type) \
{ \
	std::string stype(type); \
	if (stype == "DENSITY") { DO_GRID(data->density.get()) } \
	else if (stype == "VELOCITY") { DO_GRID(data->velocity.get()) } \
	else if (stype == "PRESSURE") { DO_GRID(data->tmp_pressure.get()) } \
	else if (stype == "DIVERGENCE") { DO_GRID(data->tmp_divergence.get()) } \
	else if (stype == "DIVERGENCE_NEW") { DO_GRID(data->tmp_divergence_new.get()) } \
	else if (stype == "FORCE") { DO_GRID(data->tmp_force.get()) } \
	else if (stype == "OBSTACLE") { DO_GRID(data->obstacle.get()) } \
	else if (stype == "PRESSURE_GRADIENT") { DO_GRID(data->tmp_pressure_gradient.get()) } \
	else if (stype == "NEIGHBOR_SOLID1") { DO_GRID(data->tmp_neighbor_solid[0].get()) } \
	else if (stype == "NEIGHBOR_SOLID2") { DO_GRID(data->tmp_neighbor_solid[1].get()) } \
	else if (stype == "NEIGHBOR_SOLID3") { DO_GRID(data->tmp_neighbor_solid[2].get()) } \
	else if (stype == "NEIGHBOR_SOLID4") { DO_GRID(data->tmp_neighbor_solid[3].get()) } \
	else if (stype == "NEIGHBOR_SOLID5") { DO_GRID(data->tmp_neighbor_solid[4].get()) } \
	else if (stype == "NEIGHBOR_SOLID6") { DO_GRID(data->tmp_neighbor_solid[5].get()) } \
	else if (stype == "NEIGHBOR_FLUID1") { DO_GRID(data->tmp_neighbor_fluid[0].get()) } \
	else if (stype == "NEIGHBOR_FLUID2") { DO_GRID(data->tmp_neighbor_fluid[1].get()) } \
	else if (stype == "NEIGHBOR_FLUID3") { DO_GRID(data->tmp_neighbor_fluid[2].get()) } \
	else if (stype == "NEIGHBOR_FLUID4") { DO_GRID(data->tmp_neighbor_fluid[3].get()) } \
	else if (stype == "NEIGHBOR_FLUID5") { DO_GRID(data->tmp_neighbor_fluid[4].get()) } \
	else if (stype == "NEIGHBOR_FLUID6") { DO_GRID(data->tmp_neighbor_fluid[5].get()) } \
	else if (stype == "NEIGHBOR_EMPTY1") { DO_GRID(data->tmp_neighbor_empty[0].get()) } \
	else if (stype == "NEIGHBOR_EMPTY2") { DO_GRID(data->tmp_neighbor_empty[1].get()) } \
	else if (stype == "NEIGHBOR_EMPTY3") { DO_GRID(data->tmp_neighbor_empty[2].get()) } \
	else if (stype == "NEIGHBOR_EMPTY4") { DO_GRID(data->tmp_neighbor_empty[3].get()) } \
	else if (stype == "NEIGHBOR_EMPTY5") { DO_GRID(data->tmp_neighbor_empty[4].get()) } \
	else if (stype == "NEIGHBOR_EMPTY6") { DO_GRID(data->tmp_neighbor_empty[5].get()) } \
	else { DO_GRID(data->density.get()) } \
} (void)0

void OpenVDB_smoke_get_draw_buffers_boxes(OpenVDBPrimitive *pdata, const char *grid, float value_scale,
                                          float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;

#define DO_GRID(grid) \
	internal::OpenVDB_get_draw_buffer_size_boxes(grid, r_numverts); \
	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3; \
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer"); \
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer"); \
	*r_normals = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB normal buffer"); \
	internal::OpenVDB_get_draw_buffers_boxes(grid, value_scale, *r_verts, *r_colors, *r_normals);

	SELECT_SMOKE_GRID(data, grid);

#undef DO_GRID
}

void OpenVDB_smoke_get_draw_buffers_needles(OpenVDBPrimitive *pdata, const char *grid, float value_scale,
                                            float (**r_verts)[3], float (**r_colors)[3], float (**r_normals)[3], int *r_numverts)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;

#define DO_GRID(grid) \
	internal::OpenVDB_get_draw_buffer_size_needles(grid, r_numverts); \
	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3; \
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer"); \
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer"); \
	*r_normals = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB normal buffer"); \
	internal::OpenVDB_get_draw_buffers_needles(grid, value_scale, *r_verts, *r_colors, *r_normals);

	SELECT_SMOKE_GRID(data, grid);

#undef DO_GRID
}

void OpenVDB_smoke_get_draw_buffers_staggered(OpenVDBPrimitive *pdata, const char *grid, float value_scale,
                                            float (**r_verts)[3], float (**r_colors)[3], int *r_numverts)
{
	internal::SmokeData *data = (internal::SmokeData *)pdata;

#define DO_GRID(grid) \
	internal::OpenVDB_get_draw_buffer_size_staggered(grid, r_numverts); \
	const size_t bufsize_v3 = (*r_numverts) * sizeof(float) * 3; \
	*r_verts = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB vertex buffer"); \
	*r_colors = (float (*)[3])MEM_mallocN(bufsize_v3, "OpenVDB color buffer"); \
	internal::OpenVDB_get_draw_buffers_staggered(grid, value_scale, *r_verts, *r_colors);

	SELECT_SMOKE_GRID(data, grid);

#undef DO_GRID
}

void OpenVDB_smoke_get_value_range(struct OpenVDBPrimitive *pdata, const char *grid, float *bg, float *min, float *max)
{
#define DO_GRID(grid) \
	internal::OpenVDB_get_grid_value_range(grid, bg, min, max);

	SELECT_SMOKE_GRID(data, grid);

#undef DO_GRID
}

#undef OPENVDB_SELECT_GRID

#endif
