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

void PoseidonData_init(PoseidonData *handle, float dh, int advection)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::init_data(data, dh, advection);
}

void PoseidonData_add_inflow(PoseidonData *handle, OpenVDBPrimitive *inflow)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::add_inflow(data, inflow);
}

void PoseidonData_add_obstacle(struct PoseidonData *handle, struct OpenVDBPrimitive *obstacle)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::add_obstacle(data, obstacle);
}

void PoseidonData_step(PoseidonData *handle, float dt)
{
	poseidon::FluidData *data = reinterpret_cast<poseidon::FluidData *>(handle);
	poseidon::step_smoke(data, dt);
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
}
