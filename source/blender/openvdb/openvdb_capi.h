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

int OpenVDB_getVersionHex(void);

struct DerivedMesh;
struct ImportMeshData;
struct Object;
struct ParticleMesherModifierData;
struct Scene;
struct VDBMeshDescr;

// Get number of vertices.
typedef int (*VDBImporter_GetNumVerts) (struct ImportMeshData *import_data);

// Get number of loops.
typedef int (*VDBImporter_GetNumLoops) (struct ImportMeshData *import_data);

// Get number of polys.
typedef int (*VDBImporter_GetNumPolys) (struct ImportMeshData *import_data);

// Get 3D coordinate of vertex with given index.
typedef void (*VDBImporter_GetVertCoord) (struct ImportMeshData *import_data, int vert_index, float coord[3]);

// Get number of adjacent vertices to the poly specified by its index.
typedef int (*VDBImporter_GetPolyNumVerts) (struct ImportMeshData *import_data, int poly_index);

// Get list of adjacent vertices to the poly specified by its index.
typedef void (*VDBImporter_GetPolyVerts) (struct ImportMeshData *import_data, int poly_index, int *verts);

typedef struct VDBMeshImporter {
	VDBImporter_GetNumVerts getNumVerts;
	VDBImporter_GetNumLoops getNumLoops;
	VDBImporter_GetNumPolys getNumPolys;
	VDBImporter_GetVertCoord getVertCoord;
	VDBImporter_GetPolyNumVerts getPolyNumVerts;
	VDBImporter_GetPolyVerts getPolyVerts;
} VDBMeshImporter;

struct VDBMeshDescr *VDB_addMesh(struct ImportMeshData *import_data,
								 VDBMeshImporter *mesh_importer);

//
// Exporter from VDB module to external storage
//

struct ExportMeshData;

// Initialize arrays for geometry.
typedef void (*VDBExporter_InitGeomArrays) (struct ExportMeshData *export_data,
											int num_verts, int num_loops, int num_polys);

// Set coordinate of vertex with given index.
typedef void (*VDBExporter_SetVert) (struct ExportMeshData *export_data,
									 int vert_index, float coord[3]);

// Set adjacent loops to the poly specified by its index.
typedef void (*VDBExporter_SetPoly) (struct ExportMeshData *export_data,
									 int poly_index, int start_loop, int num_loops);

// Set vertex which is adjacent to the loop with given index.
typedef void (*VDBExporter_SetLoop) (struct ExportMeshData *export_data,
									 int loop_index, int vertex);

typedef struct VDBMeshExporter {
	VDBExporter_InitGeomArrays initGeomArrays;
	VDBExporter_SetVert setVert;
	VDBExporter_SetPoly setPoly;
	VDBExporter_SetLoop setLoop;
} VDBMeshExporter;

bool OpenVDB_performParticleSurfacing(struct Scene *scene, struct Object *ob,
									  struct ParticleMesherModifierData *pmmd,
									  struct VDBMeshDescr *mask_mesh,
									  struct VDBMeshDescr **output_mesh);

void VDB_deleteMesh(struct VDBMeshDescr *mesh_descr);

void VDB_exportMesh(struct VDBMeshDescr *mesh_descr,
					struct VDBMeshExporter *mesh_exporter,
					struct ExportMeshData *export_data);

#ifdef __cplusplus
}
#endif

#endif // __OPENVDB_CAPI_H__
