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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct OpenVDBInternalNode2;
struct OpenVDBPrimitive;

typedef void (*OpenVDBLeafNodeBufferCb)(void *userdata, const float *buffer);

float *OpenVDB_get_texture_buffer(
        struct OpenVDBPrimitive *prim,
        int res[3], float bbmin[3], float bbmax[3]);

void OpenVDB_get_draw_buffers_nodes(
        struct OpenVDBPrimitive *prim,
        float (**r_verts)[3],
        float (**r_colors)[3],
        int *r_numverts);

void OpenVDB_get_draw_buffers_boxes(
        struct OpenVDBPrimitive *prim,
        float value_scale,
        float (**r_verts)[3],
        float (**r_colors)[3],
        float (**r_normals)[3],
        int *r_numverts);

void OpenVDB_get_draw_buffers_needles(
        struct OpenVDBPrimitive *prim,
        float value_scale,
        float (**r_verts)[3],
        float (**r_colors)[3],
        float (**r_normals)[3],
        int *r_numverts);

void OpenVDB_get_draw_buffers_staggered(
        struct OpenVDBPrimitive *prim,
        float value_scale,
        float (**r_verts)[3],
        float (**r_colors)[3],
        int *r_numverts);

void OpenVDB_get_value_range(
        struct OpenVDBPrimitive *prim,
        float *bg, float *min, float *max);

void OpenVDB_get_internal_nodes_count(
        struct OpenVDBPrimitive *prim,
        int (**r_nodes_counts),
        int *r_num_atlas,
        const struct OpenVDBInternalNode2 ***r_nodes_handles,
        int *r_num_nodes);

void OpenVDB_get_leaf_buffers(
        const struct OpenVDBInternalNode2 *node_handle,
        OpenVDBLeafNodeBufferCb cb,
        void *userdata);

void OpenVDB_get_node_bounds(
        const struct OpenVDBPrimitive *prim,
        const struct OpenVDBInternalNode2 *node_handle,
        float bbmin[3], float bbmax[3]);

void OpenVDB_node_get_leaf_indices(
        const struct OpenVDBInternalNode2 *node_handle,
        int **indirection_map,
        int *leaf_index,
        int internal_node_index);

#ifdef __cplusplus
}
#endif
