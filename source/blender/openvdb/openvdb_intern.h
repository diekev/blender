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

#ifndef __OPENVDB_UTIL_H__
#define __OPENVDB_UTIL_H__

#include <openvdb/openvdb.h>

#include "openvdb_capi.h"

class ParticleList;

struct DerivedMesh;
struct Object;
struct ParticleMesherModifierData;
struct ParticleSystem;
struct Scene;

typedef struct VDBMeshDescr {
	std::vector<openvdb::Vec3s> points;
	std::vector<openvdb::Vec3I> tris;
	std::vector<openvdb::Vec4I> polys;
} VDBMeshDescr;

struct LevelSetFilterSettings {
	LevelSetFilterSettings()
	    : type(0)
	    , accuracy(0)
	    , iterations(3)
	    , width(1)
	    , offset(1.0f)
	{}

	short type, accuracy;
	int iterations;
	int width;
	float offset;
};

struct ParticlesToLevelSetSettings {
	ParticlesToLevelSetSettings()
	    : voxel_size(0.2f)
	    , min_part_radius(1.5f)
	    , half_width(0.75f)
	    , part_scale_factor(1.0f)
	    , part_vel_factor(1.0f)
	    , trail_size(1.0f)
	    , mask_width(0.0f)
	    , generate_trails(false)
	    , generate_mask(false)
	{}

	float voxel_size;
	float min_part_radius;
	float half_width;
	float part_scale_factor;
	float part_vel_factor;
	float trail_size;
	float mask_width;
	bool generate_trails;
	bool generate_mask;
};

struct MeshToVolumeSettings {
	MeshToVolumeSettings()
	    : ext_band(3.0f)
	    , int_band(3.0f)
	{}

	float ext_band;
	float int_band;
};

struct VolumeToMeshSettings {
	VolumeToMeshSettings()
	    : isovalue(0.0f)
	    , adaptivity(0.0f)
	    , mask_offset(0.0f)
	    , invert_mask(false)
	{}

	float isovalue;
	float adaptivity;
	float mask_offset;
	bool invert_mask;
};

template<typename Settings>
LevelSetFilterSettings get_level_set_filter_settings(Settings *settings);

template<typename Settings>
ParticlesToLevelSetSettings get_particles_level_set_settings(Settings *settings);

template<typename Settings>
MeshToVolumeSettings get_mesh_to_volume_settings(Settings *settings);

template<typename Settings>
VolumeToMeshSettings get_volume_to_mesh_settings(Settings *settings);

ParticleList create_particle_list(Scene *scene, Object *ob, ParticleSystem *psys,
                                  openvdb::math::Transform::Ptr transform,
                                  float radius_scale, float velocity_scale);

void OpenVDB_filter_level_set(openvdb::FloatGrid::Ptr level_set,
							  openvdb::FloatGrid::Ptr filter_mask,
							  std::vector<LevelSetFilterSettings> &settings);

void OpenVDB_from_particles(openvdb::FloatGrid::Ptr level_set,
                            openvdb::FloatGrid::Ptr mask_grid,
							const ParticlesToLevelSetSettings &settings,
                            ParticleList Pa);

openvdb::FloatGrid::Ptr OpenVDB_from_DerivedMesh(DerivedMesh *dm,
												 const MeshToVolumeSettings &settings,
												 openvdb::math::Transform::Ptr transform);

DerivedMesh *OpenVDB_to_DerivedMesh(openvdb::FloatGrid::Ptr level_set,
                                    openvdb::FloatGrid::Ptr mask_grid,
                                    openvdb::math::Transform::Ptr transform,
									const VolumeToMeshSettings &settings);

openvdb::FloatGrid::Ptr OpenVDB_from_polygons(struct VDBMeshDescr *dm,
											  const MeshToVolumeSettings &settings,
											  openvdb::math::Transform::Ptr transform);

struct VDBMeshDescr *OpenVDB_to_polygons(openvdb::FloatGrid::Ptr level_set,
										 openvdb::FloatGrid::Ptr mask_grid,
										 openvdb::math::Transform::Ptr transform,
										 const VolumeToMeshSettings &settings);

#endif // __OPENVDB_UTIL_H__
