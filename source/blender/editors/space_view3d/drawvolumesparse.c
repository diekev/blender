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
 * The Original Code is Copyright (C) 2012-2014 Matthew Reid.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_volume_types.h"

#include "BLI_math.h"

#include "GPU_draw.h"
#include "GPU_texture.h"

#include "view3d_intern.h"

#include "openvdb_capi.h"
#include "openvdb_render_capi.h"

/* ************************* Texture Atlas ********************** */

typedef struct AtlasBuilder {
	int item_count;
	int item_count_x;
	int item_count_y;
	int item_count_z;
	int item_width;

	float *buffer;
} AtlasBuilder;

static void create_atlas_builder(AtlasBuilder *builder)
{
	builder->item_count = 0;
	builder->item_width = 8;

	builder->item_count_x = ceil(pow(4096.0f, 1.0 / 3.0));
	builder->item_count_x = power_of_2_max_u(builder->item_count_x);

	builder->item_count_y = ceil(sqrt(4096.0f / builder->item_count_x));
	builder->item_count_y = power_of_2_max_u(builder->item_count_y);

	builder->item_count_z = ceil((4096.0f / (builder->item_count_x * builder->item_count_y)));
	builder->item_count_z = power_of_2_max_u(builder->item_count_z);

	builder->buffer = MEM_mallocN(sizeof(float) * 4096 * 8 * 8 * 8,
	                              "Internal node buffer");
}

static void free_atlas_builder(AtlasBuilder *builder)
{
	MEM_freeN(builder->buffer);
}

static void atlas_add_texture(void *data, const float *leaf_buffer)
{
	AtlasBuilder *builder = data;

	const int fx = (builder->item_count % builder->item_count_x) * 8;
	const int fy = ((builder->item_count / builder->item_count_x) % builder->item_count_y) * 8;
	const int fz = (builder->item_count / (builder->item_count_x * builder->item_count_y)) * 8;

	int index = 0;
	for (int z = 0; z < 8; z++) {
		for (int y = 0; y < 8; y++) {
			float *buffer = &builder->buffer[fx + (fy + y) * fx + (fz + z) * fx * fy];

			for (int x = 0; x < 8; x++, index++) {
				*buffer++ = leaf_buffer[index];
			}
		}
	}

	builder->item_count++;
}

static GPUTexture *build_atlas(AtlasBuilder *builder)
{
	GPUTexture *tex = GPU_texture_create_3D(
	                      builder->item_count_x * builder->item_width,
	                      builder->item_count_y * builder->item_width,
	                      builder->item_count_z * builder->item_width,
	                      1,
	                      builder->buffer);

	return tex;
}

static GPUTexture *build_node_indirection_texture(
        int *internal_node_counts, int num_atlases,
        const struct OpenVDBInternalNode2 **node_handles, int num_nodes)
{
	int internal_node_index = 0;
	const int max_node_leaf_count = 4096;
	const int map_size = max_node_leaf_count * num_nodes;

	int *indirection_map = MEM_mallocN(sizeof(int) * map_size,
	                                   "Indirection Map");

	memset(indirection_map, -1, sizeof(int) * map_size);

	for (int i = 0; i < num_atlases; i++) {
		int leaf_index = 0;
		int node_index = 0;

		while (node_index < internal_node_counts[i]) {
			OpenVDB_node_get_leaf_indices(node_handles[internal_node_index], &indirection_map, &leaf_index, internal_node_index);

			++node_index;
			++internal_node_index;
		}
	}

	GPUTexture *tex = GPU_texture_create_1D_int(map_size, indirection_map, NULL);

	MEM_freeN(indirection_map);

	return tex;
}

void create_volume_texture_atlas(VolumeData *data)
{
	int *atlas_nodes_counts = NULL;
	int num_atlases = 0;
	const struct OpenVDBInternalNode2 **node_handles;
	int num_nodes = 0;

	/* Calculate texture atlas sizes. */
	OpenVDB_get_internal_nodes_count(data->prim,
	                                 &atlas_nodes_counts, &num_atlases,
	                                 &node_handles, &num_nodes);

	data->draw_nodes = MEM_mallocN(sizeof(VolumeDrawNode *) * num_nodes,
	                               "Volume Data Draw Nodes");

	data->num_draw_nodes = num_nodes;

	/* Build node indirection texture. */
	GPUTexture *indirection_tex = build_node_indirection_texture(
	                                  atlas_nodes_counts, num_atlases,
	                                  node_handles, num_nodes);

#if 0
	printf("Number of internal nodes: %d\n", num_internal_nodes);
#endif

	int current_node_index = 0;
	for (int i = 0; i < num_atlases; i++) {
		AtlasBuilder builder;
		create_atlas_builder(&builder);

#if 0
		printf("Atlas %d: number of nodes: %d\n", i, atlas_nodes_counts[i]);
		printf("Builder %d: item per dim: %d, %d, %d\n", i,
		       builder.item_count_x, builder.item_count_y, builder.item_count_z);
#endif

		/* fill up this atlas with leaves from the nodes until it is full */

		const int first_node_index = current_node_index;
		while (current_node_index - first_node_index < atlas_nodes_counts[i]) {
			/* Create renderable box for this internal node */
			VolumeDrawNode *drawnode = GPU_volume_node_create(indirection_tex,
			                                                  current_node_index > 0);

			OpenVDB_get_node_bounds(data->prim, node_handles[current_node_index],
			                        drawnode->bbmin, drawnode->bbmax);
#if 0
			printf("Internal node %d (%p)\n", i, node_handles[current_node_index]);
			print_v3("center", drawnode.center);
			print_v3("size", drawnode.size);
#endif

			/* Add all leaves of this internal node to atlas */
			OpenVDB_get_leaf_buffers(node_handles[current_node_index],
			                         atlas_add_texture, (void *)(&builder));

			data->draw_nodes[current_node_index] = drawnode;

			current_node_index++;
		}

		GPUTexture *atlas_tex = build_atlas(&builder);

		for (int j = 0; j < atlas_nodes_counts[i]; j++) {
			const int index = first_node_index + j;
			data->draw_nodes[index]->leaf_atlas = atlas_tex;
			data->draw_nodes[index]->first_node_index = index;
			data->draw_nodes[index]->max_leaves_per_atlas_dim[0] = builder.item_count_x;
			data->draw_nodes[index]->max_leaves_per_atlas_dim[1] = builder.item_count_y;
			data->draw_nodes[index]->max_leaves_per_atlas_dim[2] = builder.item_count_z;

			if (j > 0) {
				GPU_texture_ref(data->draw_nodes[index]->leaf_atlas);
			}
		}

		printf("Atlas %d: full at %.2f%%\n", i, (builder.item_count / 40.96f));
		free_atlas_builder(&builder);
	}

	/* cleanup */

	if (atlas_nodes_counts) {
		MEM_freeN(atlas_nodes_counts);
	}

	if (node_handles) {
		MEM_freeN(node_handles);
	}
}
