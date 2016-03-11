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

#include "poseidon_capi.h"

#include "../guardedalloc/MEM_guardedalloc.h"

#include "poseidon/poseidon.h"

typedef struct PoseidonData { int unused; } PoseidonData;

PoseidonData *PoseidonData_create()
{
	poseidon::FluidData *data = new poseidon::FluidData;
	return reinterpret_cast<PoseidonData *>(data);
}

void PoseidonData_free(PoseidonData *handle)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	delete data;
}

void PoseidonData_init(PoseidonData *handle, float dh)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::init_data(data, dh);
}

void PoseidonData_add_inflow(PoseidonData *handle, OpenVDBPrimitive *inflow)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::add_inflow(data, inflow);
}

void PoseidonData_add_particle_inflow(PoseidonData *handle, OpenVDBPrimitive *inflow)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::add_particle_inflow(data, inflow);
}

void PoseidonData_add_obstacle(PoseidonData *handle, OpenVDBPrimitive *obstacle)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::add_obstacle(data, obstacle);
}

void PoseidonData_add_domain_walls(PoseidonData *handle, float min[3], float max[3])
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::create_domain_walls(data, openvdb::BBoxd(min, max));
}

void PoseidonData_step(PoseidonData *handle, float dt, int advection, int limiter)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::step_smoke(data, dt, advection, limiter);
}

void PoseidonData_step_liquid(PoseidonData *handle, float dt, int point_integration)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::step_flip(data, dt, point_integration);
}

void PoseidonData_get_particle_draw_buffer(PoseidonData *handle, int *r_numpoints, float (**r_buffer)[3])
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);

	if (data->particles.size() == 0) {
		*r_numpoints = 0;
		*r_buffer = nullptr;
		return;
	}

	*r_numpoints = data->particles.size();
	*r_buffer = (float (*)[3])MEM_mallocN((*r_numpoints) * sizeof(float) * 3, "Poseidon particle buffer");

	for (int i = 0; i < *r_numpoints; ++i) {
		auto p = data->particles[i];
		(*r_buffer)[i][0] = p.x();
		(*r_buffer)[i][1] = p.y();
		(*r_buffer)[i][2] = p.z();
	}
}

OpenVDBPrimitive *Poseidon_get_field(PoseidonData *handle, int index)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);

	switch (index) {
		case POSEIDON_FIELD_DENSITY:
			return &data->density;
		case POSEIDON_FIELD_VELOCITY:
			return &data->velocity;
		case POSEIDON_FIELD_COLLISION:
			return &data->collision;
		case POSEIDON_FIELD_PRESSURE:
			return &data->pressure;
		case POSEIDON_FIELD_TEMPERATURE:
			return &data->temperature;
		case POSEIDON_FIELD_FLAGS:
			return &data->flags;
		default:
			assert(false);
	}

	return NULL;
}
