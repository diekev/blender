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

#include "poseidon.h"

#include <openvdb/tools/Composite.h>

#include "advection.h"
#include "forces.h"
#include "types.h"
#include "util_smoke.h"

#include "openvdb_util.h"

namespace poseidon {

void init_data(FluidData * const data, float voxel_size, int advection)
{
	data->dh = voxel_size;
	data->advection_scheme = advection;
	data->xform = *openvdb::math::Transform::createLinearTransform(data->dh);
	data->max_vel = 0.0f;
	data->dt = 0.1f;

	ScalarGrid::Ptr density = openvdb::gridPtrCast<ScalarGrid>(data->density.getGridPtr());
	ScalarGrid::Ptr temperature = openvdb::gridPtrCast<ScalarGrid>(data->temperature.getGridPtr());
	ScalarGrid::Ptr pressure = openvdb::gridPtrCast<ScalarGrid>(data->pressure.getGridPtr());
	VectorGrid::Ptr velocity = openvdb::gridPtrCast<VectorGrid>(data->velocity.getGridPtr());
	openvdb::BoolGrid::Ptr obstacle = openvdb::gridPtrCast<openvdb::BoolGrid>(data->collision.getGridPtr());

	initialize_field<ScalarGrid>(density, "density", data->xform);
	initialize_field<openvdb::BoolGrid>(obstacle, "obstacle", data->xform);
	initialize_field<ScalarGrid>(pressure, "pressure", data->xform);
	initialize_field<ScalarGrid>(temperature, "temperature", data->xform);
	initialize_field<VectorGrid>(velocity, "velocity", data->xform,
	                             openvdb::VEC_CONTRAVARIANT_RELATIVE,
	                             openvdb::GRID_STAGGERED);
}

static void advect_semi_lagrange(FluidData * const data, float dt)
{
	Timer(__func__);

	ScalarGrid::Ptr density = openvdb::gridPtrCast<ScalarGrid>(data->density.getGridPtr());
	ScalarGrid::Ptr temperature = openvdb::gridPtrCast<ScalarGrid>(data->temperature.getGridPtr());
	VectorGrid::Ptr velocity = openvdb::gridPtrCast<VectorGrid>(data->velocity.getGridPtr());

	typedef openvdb::tools::Sampler<1, false> Sampler;

	openvdb::tools::VolumeAdvection<VectorGrid, false> advector(*velocity);
	advector.setIntegrator(advection_scheme(data->advection_scheme));

	data->max_vel = advector.getMaxVelocity();
	const int n = advector.getMaxDistance(*density, dt);

	if (n > 20) {
		OPENVDB_THROW(openvdb::RuntimeError, "Advection distance is too high!");
	}

	ScalarGrid::Ptr sresult;

	sresult = advector.advect<ScalarGrid, Sampler>(*density, dt);
	density.swap(sresult);

	sresult = advector.advect<ScalarGrid, Sampler>(*temperature, dt);
	temperature.swap(sresult);

	VectorGrid::Ptr result;
	result = advector.advect<VectorGrid, Sampler>(*velocity, dt);
	velocity.swap(result);
}

void step_smoke(FluidData * const data, float dt)
{
	ScalarGrid::Ptr density = openvdb::gridPtrCast<ScalarGrid>(data->density.getGridPtr());
	ScalarGrid::Ptr temperature = openvdb::gridPtrCast<ScalarGrid>(data->temperature.getGridPtr());
	ScalarGrid::Ptr pressure = openvdb::gridPtrCast<ScalarGrid>(data->pressure.getGridPtr());
	openvdb::Int32Grid::Ptr flags = openvdb::gridPtrCast<openvdb::Int32Grid>(data->flags.getGridPtr());
	VectorGrid::Ptr velocity = openvdb::gridPtrCast<VectorGrid>(data->velocity.getGridPtr());
	openvdb::BoolGrid::Ptr obstacle = openvdb::gridPtrCast<openvdb::BoolGrid>(data->collision.getGridPtr());

	velocity->topologyUnion(*density);
	flags = build_flag_grid(density, obstacle);

	/* start step */

	advect_semi_lagrange(data, dt);

	set_neumann_boundary(*velocity, flags);
	add_buoyancy(dt, velocity, density, temperature, flags);

	solve_pressure(dt, *velocity, *pressure, *flags);
	set_neumann_boundary(*velocity, flags);
}

void add_inflow(FluidData * const data, OpenVDBPrimitive *inflow_prim)
{
	Timer(__func__);

	using namespace openvdb;

	auto density = gridPtrCast<ScalarGrid>(data->density.getGridPtr());
	auto inflow = gridPtrCast<ScalarGrid>(inflow_prim->getGridPtr());

	tools::compMax(*density, *inflow);
}

void add_obstacle(FluidData * const data, OpenVDBPrimitive *obstacle_prim)
{
	Timer(__func__);

	using namespace openvdb;

	auto collision = gridPtrCast<openvdb::BoolGrid>(data->collision.getGridPtr());
	auto obstacle = gridPtrCast<openvdb::BoolGrid>(obstacle_prim->getGridPtr());

	tools::compMax(*collision, *obstacle);
}

}  /* namespace poseidon */
