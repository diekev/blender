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
#include <openvdb/tools/LevelSetUtil.h>

#include "advection.h"
#include "forces.h"
#include "types.h"
#include "util_smoke.h"

#include "openvdb_util.h"

namespace poseidon {

void init_data(FluidData * const data, float voxel_size)
{
	data->dh = voxel_size;
	data->advection_scheme = ADVECT_SEMI;
	data->xform = *openvdb::math::Transform::createLinearTransform(data->dh);
	data->max_vel = 0.0f;
	data->dt = 0.1f;

	ScalarGrid::Ptr density;
	initialize_field<ScalarGrid>(density, "density", data->xform);
	data->density.setGridPtr(density);

	openvdb::BoolGrid::Ptr obstacle;
	initialize_field<openvdb::BoolGrid>(obstacle, "obstacle", data->xform);
	data->collision.setGridPtr(obstacle);

	ScalarGrid::Ptr pressure;
	initialize_field<ScalarGrid>(pressure, "pressure", data->xform);
	data->pressure.setGridPtr(pressure);

	ScalarGrid::Ptr temperature;
	initialize_field<ScalarGrid>(temperature, "temperature", data->xform);
	data->temperature.setGridPtr(temperature);

	VectorGrid::Ptr velocity;
	initialize_field<VectorGrid>(velocity, "velocity", data->xform,
	                             openvdb::VEC_CONTRAVARIANT_RELATIVE,
	                             openvdb::GRID_STAGGERED);
	data->velocity.setGridPtr(velocity);
}

void create_domain_walls(FluidData * const data, const openvdb::math::BBox<openvdb::Vec3d> &wbbox)
{
	using namespace openvdb;
	using namespace openvdb::math;

	auto collision = gridPtrCast<openvdb::BoolGrid>(data->collision.getGridPtr());

	Transform xform = collision->transform();
	CoordBBox bbox = xform.worldToIndexCellCentered(wbbox);

	auto min = bbox.min();
	auto max = bbox.max();

	/* X slabs */
	CoordBBox bbox_xmin(min, Coord(min[0], max[1], max[2]));
	collision->tree().fill(bbox_xmin, true);

	CoordBBox bbox_xmax(Coord(max[0], min[1], min[2]), max);
	collision->tree().fill(bbox_xmax, true);

	/* Y slabs */
	CoordBBox bbox_ymin(min, Coord(max[0], min[1], max[2]));
	collision->tree().fill(bbox_ymin, true);

	CoordBBox bbox_ymax(Coord(min[0], max[1], min[2]), max);
	collision->tree().fill(bbox_ymax, true);

	/* Z slabs */
	CoordBBox bbox_zmin(min, Coord(max[0], max[1], min[2]));
	collision->tree().fill(bbox_zmin, true);

	CoordBBox bbox_zmax(Coord(min[0], min[1], max[2]), max);
	collision->tree().fill(bbox_zmax, true);
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
	data->density.setGridPtr(sresult);

//	density.swap(sresult);

	sresult = advector.advect<ScalarGrid, Sampler>(*temperature, dt);
	data->temperature.setGridPtr(sresult);
//	temperature.swap(sresult);

	VectorGrid::Ptr result;
	result = advector.advect<VectorGrid, Sampler>(*velocity, dt);
	data->velocity.setGridPtr(result);
//	velocity.swap(result);
}

void step_smoke(FluidData * const data, float dt, int advection)
{
	ScalarGrid::Ptr density = openvdb::gridPtrCast<ScalarGrid>(data->density.getGridPtr());
	ScalarGrid::Ptr temperature = openvdb::gridPtrCast<ScalarGrid>(data->temperature.getGridPtr());
	ScalarGrid::Ptr pressure = openvdb::gridPtrCast<ScalarGrid>(data->pressure.getGridPtr());
	VectorGrid::Ptr velocity = openvdb::gridPtrCast<VectorGrid>(data->velocity.getGridPtr());
	openvdb::BoolGrid::Ptr obstacle = openvdb::gridPtrCast<openvdb::BoolGrid>(data->collision.getGridPtr());

	velocity->topologyUnion(*density);

	auto flags = build_flag_grid(density, obstacle);
	data->flags.setGridPtr(flags);

	/* start step */
	data->advection_scheme = advection;

	advect_semi_lagrange(data, dt);

	density = openvdb::gridPtrCast<ScalarGrid>(data->density.getGridPtr());
	temperature = openvdb::gridPtrCast<ScalarGrid>(data->temperature.getGridPtr());
	velocity = openvdb::gridPtrCast<VectorGrid>(data->velocity.getGridPtr());

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
	auto temperature = gridPtrCast<ScalarGrid>(data->temperature.getGridPtr());
	auto inflow = gridPtrCast<ScalarGrid>(inflow_prim->getGridPtr());

	tools::sdfToFogVolume(*inflow);

//	density->topologyUnion(*inflow);

//	tools::compMax(*density, *(inflow->copyGrid(CopyPolicy::CP_NEW)));
//	tools::compMax(*temperature, *(inflow->copyGrid(CopyPolicy::CP_NEW)));

	ScalarAccessor dacc = density->getAccessor();
	ScalarAccessor tacc = temperature->getAccessor();

	typedef FloatTree::LeafNodeType LeafType;

	for (FloatTree::LeafIter lit = inflow->tree().beginLeaf(); lit; ++lit) {
		LeafType &leaf = *lit;

		for (typename LeafType::ValueOnIter it = leaf.beginValueOn(); it; ++it) {
			dacc.setValue(it.getCoord(), 1.0f);
			tacc.setValue(it.getCoord(), 1.0f);
		}
	}
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
