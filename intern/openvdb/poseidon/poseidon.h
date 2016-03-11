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
 * The Original Code is Copyright (C) 2015-2016 Kevin Dietrich.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#pragma once

#include "intern/openvdb_primitive.h"
#include "particles.h"

namespace poseidon {

struct FluidData {
	OpenVDBPrimitive velocity;
	OpenVDBPrimitive density;
	OpenVDBPrimitive temperature;
	OpenVDBPrimitive pressure;
	OpenVDBPrimitive collision;
	OpenVDBPrimitive flags;
	OpenVDBPrimitive index_grid;

	ParticleList particles;

	float dt;  /* time step */
	float dh;  /* voxel size, sometimes refered to as dx or dTau */
	int advection_scheme;
	int limiter;
	float max_vel;
	openvdb::math::Transform xform;
};

void step_smoke(FluidData * const data, float dt, int advection, int limiter);
void step_flip(FluidData * const data, float dt, int point_integration);
void init_data(FluidData * const data, float voxel_size);
void add_inflow(FluidData * const data, OpenVDBPrimitive *inflow_prim);
void add_particle_inflow(FluidData * const data, OpenVDBPrimitive *inflow_prim);
void add_obstacle(FluidData * const data, OpenVDBPrimitive *obstacle_prim);
void create_domain_walls(FluidData * const data, const openvdb::math::BBox<openvdb::Vec3d> &wbbox);

}  /* namespace poseidon */
