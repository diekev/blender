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
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/PointAdvect.h>
#include <openvdb/tools/PointScatter.h>

#include "globals.h"
#include "particles.h"
#include "types.h"
#include "openvdb_util.h"

void create_particles(openvdb::FloatGrid::Ptr level_set, ParticleList &particles)
{
	using namespace openvdb;

	Timer(__func__);

	const int part_per_cell = 8;
	std::mt19937 rng(19937);

	// create bool mask of inside region
	BoolGrid::Ptr interior = tools::sdfInteriorMask(*level_set);

	particles.reserve(interior->activeVoxelCount() * part_per_cell);

	tools::DenseUniformPointScatter<ParticleList, std::mt19937> scatter(particles, part_per_cell, rng);
	scatter(*interior);

	std::cout << "Particles size: " << particles.size() << "\n";
	assert(particles.size() != 0);
}

struct RasterizeOp {
	const ParticleList &m_particles;
	const float m_radius;
	float m_weight_sum;
	ParticleList::value_type m_vel_sum;
	float m_dens_sum;
	const float m_inv_radius;

	RasterizeOp(ParticleList &particles, const float radius)
	    : m_particles(particles)
	    , m_radius(radius)
	    , m_weight_sum(0.0)
	    , m_vel_sum(0.0)
	    , m_dens_sum(0.0f)
	    , m_inv_radius(1.0f / radius)
	{}

	void reset()
	{
		m_weight_sum = 0.0f;
		m_vel_sum = ParticleList::value_type::zero();
	}

	void operator()(const float dist_sqr, const size_t index)
	{
		const float weight = 1.0f - openvdb::math::Sqrt(dist_sqr) * m_inv_radius;
		m_weight_sum += weight;
		m_vel_sum += weight * m_particles.at(index)->vel;
		m_dens_sum += weight * m_particles.at(index)->density;
	}

	ParticleList::value_type velocity() const
	{
		return (m_weight_sum > 0.0f) ? m_vel_sum / m_weight_sum : ParticleList::value_type::zero();
	}

	float density() const
	{
		return (m_weight_sum > 0.0f) ? m_dens_sum / m_weight_sum : 0.0f;
	}
};

void rasterize_particles(ParticleList &particles,
                         const PointIndexGrid &index_grid,
                         openvdb::Vec3SGrid &velocity,
                         openvdb::FloatGrid &density)
{
	using namespace openvdb;
	using namespace openvdb::math;

	Timer(__func__);

	const float radius = velocity.transform().voxelSize()[0] * 2.0f;
	RasterizeOp op(particles, radius);

	/* prepare grids for insertion of values */
	velocity.topologyUnion(index_grid);
	density.topologyUnion(index_grid);

	VectorAccessor vacc = velocity.getAccessor();
	ScalarAccessor dacc = density.getAccessor();

	tools::PointIndexFilter<ParticleList> filter(particles,
	                                             index_grid.tree(),
	                                             index_grid.transform());

	Transform xform = velocity.transform();

	for (size_t n = 0, N = particles.size(); n < N; ++n) {
		Vec3s pos = particles[n];
		Coord co = xform.worldToIndexCellCentered(pos);

		op.reset();
		filter.searchAndApply(pos, radius, op);

		vacc.setValue(co, op.velocity());
		dacc.setValue(co, op.density());
	}
}

void advect_particles(ParticleList &particles, openvdb::Vec3SGrid::Ptr velocity, const float dt)
{
	using namespace openvdb;

	Timer(__func__);

	tools::PointAdvect<Vec3SGrid, ParticleList, true> integrator(*velocity);

	/* perform forward euler integration */
	integrator.setIntegrationOrder(1);
	integrator.advect(particles, dt);
}

void resample_particles(ParticleList &particles)
{
	Timer(__func__);

	tbb::parallel_for(tbb::blocked_range<size_t>(0, particles.size()),
	                  [&](const tbb::blocked_range<size_t>& r)
	{
		for (size_t i = r.begin(); i != r.end(); ++i) {
			Particle *particle = particles.at(i);
			particle->vel = particle->pos - particle->pos_t;
			particle->pos_t = particle->pos;
			particle->density *= (1.0f - (1.0f / 5.0f));
		}
	});
}

void interpolate_pic_flip(openvdb::Vec3SGrid::Ptr &velocity,
                          openvdb::Vec3SGrid::Ptr &velocity_old,
                          ParticleList &particles)
{
	using namespace openvdb;
	using namespace openvdb::math;

	Timer(__func__);

	/* flip ratio */
	const float alpha = 0.95f;

	tbb::parallel_for(tbb::blocked_range<size_t>(0, particles.size()),
	                  [&](const tbb::blocked_range<size_t>& r)
	{
		LinearVectorSampler sampler_new(velocity->getConstAccessor(), velocity->transform());
		LinearVectorSampler sampler_old(velocity_old->getConstAccessor(), velocity_old->transform());

		for (size_t i = r.begin(); i != r.end(); ++i) {
			Particle *p = particles.at(i);

			const Vec3s pos = p->pos;
			const Vec3s v     =     sampler_new.wsSample(pos);
			const Vec3s delta = v - sampler_old.wsSample(pos);

			p->vel = alpha * (p->vel + delta) + (1.0f - alpha) * v;
		}
	});
}
