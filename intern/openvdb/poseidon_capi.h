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

#ifdef __cplusplus
extern "C" {
#endif

struct PoseidonData;
struct OpenVDBPrimitive;

enum {
	POSEIDON_FIELD_DENSITY = 0,
	POSEIDON_FIELD_VELOCITY,
	POSEIDON_FIELD_COLLISION,
	POSEIDON_FIELD_PRESSURE,
	POSEIDON_FIELD_TEMPERATURE,
	POSEIDON_FIELD_FLAGS,
};

enum {
	ADVECT_SEMI = 0,
	ADVECT_MID,
	ADVECT_RK3,
	ADVECT_RK4,
	ADVECT_MAC,
	ADVECT_BFECC
};

struct PoseidonData *PoseidonData_create(void);
void PoseidonData_free(struct PoseidonData *handle);

void PoseidonData_init(struct PoseidonData *handle, float dh);

void PoseidonData_add_inflow(struct PoseidonData *handle, struct OpenVDBPrimitive *inflow);
void PoseidonData_add_obstacle(struct PoseidonData *handle, struct OpenVDBPrimitive *obstacle);
void PoseidonData_add_domain_walls(struct PoseidonData *handle, float min[3], float max[3]);

void PoseidonData_step(struct PoseidonData *handle, float dt, int advection);

struct OpenVDBPrimitive *Poseidon_get_field(struct PoseidonData *data, int index);

#ifdef __cplusplus
}
#endif
