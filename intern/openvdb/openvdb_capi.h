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

#ifndef __OPENVDB_CAPI_H__
#define __OPENVDB_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ExportMeshData;
struct ImportMeshData;
struct OpenVDBPrimitive;
struct ParticleList;
struct VDBMeshDescr;

/* Importer from external storage to VDB module */

typedef struct VDBMeshImporter {
	int (*getNumVerts) (struct ImportMeshData *import_data);
	int (*getNumLoops) (struct ImportMeshData *import_data);
	int (*getNumPolys) (struct ImportMeshData *import_data);
	void (*getVertCoord) (struct ImportMeshData *import_data, int vert_index, float coord[3]);
	int (*getPolyNumVerts) (struct ImportMeshData *import_data, int poly_index);
	void (*getPolyVerts) (struct ImportMeshData *import_data, int poly_index, int *verts);
} VDBMeshImporter;

struct VDBMeshDescr *VDB_addMesh(struct ImportMeshData *import_data,
								 VDBMeshImporter *mesh_importer);

/* Exporter from VDB module to external storage */

typedef struct VDBMeshExporter {
	void (*initGeomArrays) (struct ExportMeshData *export_data, int num_verts, int num_loops, int num_polys);
	void (*setVert) (struct ExportMeshData *export_data, int vert_index, float coord[3]);
	void (*setPoly) (struct ExportMeshData *export_data, int poly_index, int start_loop, int num_loops);
	void (*setLoop) (struct ExportMeshData *export_data, int loop_index, int vertex);
} VDBMeshExporter;

enum {
	LEVEL_FILTER_MEDIAN    = 0,
	LEVEL_FILTER_MEAN      = 1,
	LEVEL_FILTER_GAUSSIAN  = 2,
	LEVEL_FILTER_MEAN_CURV = 3,
	LEVEL_FILTER_LAPLACIAN = 4,
	LEVEL_FILTER_OFFSET    = 5,
};

enum {
	LEVEL_FILTER_ACC_FISRT   = 0,
	LEVEL_FILTER_ACC_SECOND  = 1,
	LEVEL_FILTER_ACC_THIRD   = 2,
	LEVEL_FILTER_ACC_WENO5   = 3,
	LEVEL_FILTER_ACC_HJWENO5 = 4,
};

int OpenVDB_getVersionHex(void);

void OpenVDB_filter_level_set(struct OpenVDBPrimitive *level_set,
                              struct OpenVDBPrimitive *filter_mask,
                              int accuracy, int type, int iterations,
                              int width, float offset);


void OpenVDB_from_particles(struct OpenVDBPrimitive *level_set,
                            struct OpenVDBPrimitive *mask_grid,
                            struct ParticleList *Pa, bool mask, float mask_width, float min_radius, bool trail, float trail_size);



struct OpenVDBPrimitive *OpenVDB_from_polygons(struct VDBMeshDescr *dm,
                                               float voxel_size, float int_band, float ext_band);

struct VDBMeshDescr *OpenVDB_to_polygons(struct OpenVDBPrimitive *level_set,
										 struct OpenVDBPrimitive *mask_grid,
                                         float isovalue, float adaptivity,
                                         float mask_offset, bool invert_mask);

void VDB_deleteMesh(struct VDBMeshDescr *mesh_descr);

void VDB_exportMesh(struct VDBMeshDescr *mesh_descr,
					struct VDBMeshExporter *mesh_exporter,
					struct ExportMeshData *export_data);

struct ParticleList *OpenVDB_create_part_list(size_t totpart, float rad_scale, float vel_scale);
void OpenVDB_part_list_free(struct ParticleList *part_list);
void OpenVDB_add_particle(struct ParticleList *part_list, struct OpenVDBPrimitive *vdb_prim, float pos[3], float rad, float vel[3]);
void OpenVDB_set_part_list_flags(struct ParticleList *part_list, const bool has_radius, const bool has_velocity);

enum {
	VDB_GRID_INVALID = 0,
    VDB_GRID_FLOAT   = 1,
    VDB_GRID_DOUBLE  = 2,
    VDB_GRID_INT32   = 3,
    VDB_GRID_INT64   = 4,
    VDB_GRID_BOOL    = 5,
    VDB_GRID_VEC3F   = 6,
    VDB_GRID_VEC3D   = 7,
    VDB_GRID_VEC3I   = 8,
};

struct OpenVDBPrimitive *create(int grid_type);
struct OpenVDBPrimitive *create_level_set(float voxel_size, float half_width);
void OpenVDBPrimitive_free(struct OpenVDBPrimitive *vdb_prim);

#ifdef __cplusplus
}
#endif

#endif /* __OPENVDB_CAPI_H__ */
